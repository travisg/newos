#include <string.h>

#include <kernel.h>
#include <sem.h>
#include <spinlock.h>
#include <int.h>
#include <timer.h>
#include <debug.h>
#include <vm.h>

#include <stage2.h>

static struct sem_entry *sems = NULL;
static area_id sem_area = 0;

static sem_id next_sem = 0;

static const char *temp_sem_name = "temp sem";

static int sem_spinlock = 0;
#define GRAB_SEM_LIST_LOCK() acquire_spinlock(&sem_spinlock)
#define RELEASE_SEM_LIST_LOCK() release_spinlock(&sem_spinlock)
#define GRAB_SEM_LOCK(s) acquire_spinlock(&(s).lock)
#define RELEASE_SEM_LOCK(s) release_spinlock(&(s).lock)

// used in functions that may put a bunch of threads in the run q at once
#define READY_THREAD_CACHE_SIZE 16

struct sem_timeout {
	sem_id     id;
	thread_id  thr_id;
};

void dump_sem_list(int argc, char **argv)
{
	int i;
	
	TOUCH(argc);TOUCH(argv);

	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].id >= 0) {
			dprintf("0x%x\tid: 0x%x\tname: '%s'\n", &sems[i], sems[i].id, sems[i].name);
		}
	}
}

static void _dump_sem_info(struct sem_entry *sem)
{
	dprintf("SEM:   0x%x\n", sem);
	dprintf("name:  '%s'\n", sem->name);
	dprintf("count: 0x%x\n", sem->count);
	dprintf("queue: head 0x%x tail 0x%x\n", sem->q.head, sem->q.tail);
}

static void dump_sem_info(int argc, char **argv)
{
	int i;

	if(argc < 2) {
		dprintf("sem: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		unsigned long num = atoul(argv[1]);
		
		if(num > vm_get_kernel_aspace()->base) {
			// XXX semi-hack
			_dump_sem_info((struct sem_entry *)num);
			return;
		} else {
			int slot = num % MAX_SEMS;
			_dump_sem_info(&sems[slot]);
			return;
		}
	}
	
	// walk through the sem list, trying to match name
	for(i=0; i<MAX_SEMS; i++) {
		if(strcmp(argv[1], sems[i].name) == 0) {
			_dump_sem_info(&sems[i]);
			return;
		}
	}
}

int sem_init(kernel_args *ka)
{
	int i;
	TOUCH(ka);
	
	// create and initialize semaphore table
	sem_area = vm_create_area(vm_get_kernel_aspace(), "sem_table", (void **)&sems,
		AREA_ANY_ADDRESS, sizeof(struct sem_entry) * MAX_SEMS, 0);
	if(sems == NULL) {
		panic("unable to allocate semaphore table!\n");
	}
	memset(sems, 0, sizeof(struct sem_entry) * MAX_SEMS);
	for(i=0; i<MAX_SEMS; i++)
		sems[i].id = -1;

	// add debugger commands
	dbg_add_command(&dump_sem_list, "sems", "Dump a list of all active semaphores");
	dbg_add_command(&dump_sem_info, "sem", "Dump info about a particular semaphore");

	return 0;
}

sem_id sem_create(int count, char *name)
{
	int i;
	int state;
	sem_id retval = -1;
	char *temp_name;
	
	state = int_disable_interrupts();
	GRAB_SEM_LIST_LOCK();
	
	// find the first empty spot
	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].id == -1) {
			// empty one
			if((next_sem % MAX_SEMS) < i) {
				// make the sem id be a multiple of the slot it's in
				next_sem += i - (next_sem % MAX_SEMS);
			}
			sems[i].id = next_sem++;
			
			sems[i].lock = 0;
			GRAB_SEM_LOCK(sems[i]);
			RELEASE_SEM_LIST_LOCK();
			
			sems[i].count = count;
			sems[i].name = (char *)temp_sem_name;
			retval = sems[i].id;
			RELEASE_SEM_LOCK(sems[i]);
						
			temp_name = (char *)kmalloc(strlen(name)+1);
			if(temp_name == NULL) {
				sem_delete(retval);
				retval = -1;
				goto err;
			}		
			strcpy(temp_name, name);

			GRAB_SEM_LOCK(sems[i]);
			if(sems[i].id == retval) {
				sems[i].name = temp_name;
				RELEASE_SEM_LOCK(sems[i]);
			} else {
				// the sem must have been deleted and possibly reused,
				// our string is now worthless
				RELEASE_SEM_LOCK(sems[i]);
				kfree(temp_name);
				retval = -1;							
			}
			break;
		}
	}
	if(i >= MAX_SEMS)
		RELEASE_SEM_LIST_LOCK();

err:
	int_restore_interrupts(state);

	return retval;
}

int sem_delete(sem_id id)
{
	int slot = id % MAX_SEMS;
	int state;
	int err = 0;
	struct thread *t;
	int released_threads = 0;
	char *old_name;
	
	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		RELEASE_SEM_LOCK(sems[slot]);
		int_restore_interrupts(state);
		dprintf("sem_delete: invalid sem_id %d\n", id);
		return -1;
	}
	
	// free any threads waiting for this semaphore
	// put them in the runq in batches, to keep the amount of time
	// spent with the thread lock held down to a minimum
	{
		struct thread *ready_threads[READY_THREAD_CACHE_SIZE];
		int ready_threads_count = 0;
			
		while((t = thread_dequeue(&sems[slot].q)) != NULL) {
			t->state = THREAD_STATE_READY;
			ready_threads[ready_threads_count++] = t;
			released_threads++;
			
			if(ready_threads_count == READY_THREAD_CACHE_SIZE) {
				// put a batch of em in the runq a once				
				GRAB_THREAD_LOCK();
				for(; ready_threads_count > 0; ready_threads_count--)
					thread_enqueue_run_q(ready_threads[ready_threads_count - 1]);
				RELEASE_THREAD_LOCK();
				// ready_threads_count is back to 0
			}
		}
		// put any leftovers in the runq
		if(ready_threads_count > 0) {
			GRAB_THREAD_LOCK();
			for(; ready_threads_count > 0; ready_threads_count--)
				thread_enqueue_run_q(ready_threads[ready_threads_count - 1]);
			RELEASE_THREAD_LOCK();
		}
	}
	sems[slot].id = -1;
	old_name = sems[slot].name;
	sems[slot].name = NULL;
	
	RELEASE_SEM_LOCK(sems[slot]);

	if(old_name != temp_sem_name)
		kfree(old_name);

	if(released_threads > 0) {
		GRAB_THREAD_LOCK();
		thread_resched();
		RELEASE_THREAD_LOCK();
	}

	int_restore_interrupts(state);
	
	return err;
}

// Called from a timer handler. Wakes up a semaphore
static void sem_timeout(void *data)
{
	struct thread *t = (struct thread *)data;
	int slot = t->blocked_sem_id % MAX_SEMS;
	int state;
	
	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);
	
//	dprintf("sem_timeout: called on 0x%x sem %d, tid %d\n", to, to->sem_id, to->thread_id);
	
	t = thread_dequeue_id(&sems[slot].q, t->id);
	if(t != NULL) {
		sems[slot].count += t->sem_count;
		t->state = THREAD_STATE_READY;
		GRAB_THREAD_LOCK();
		thread_enqueue_run_q(t);
		RELEASE_THREAD_LOCK();
	}

	RELEASE_SEM_LOCK(sems[slot]);
	int_restore_interrupts(state);
}

int sem_acquire(sem_id id, int count)
{
	return sem_acquire_etc(id, count, 0, 0);
}

int sem_acquire_etc(sem_id id, int count, int flags, long long timeout)
{
	int slot = id % MAX_SEMS;
	int state;
	int err = 0;
	
	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);
	
	if(sems[slot].id != id) {
		dprintf("sem_acquire_etc: invalid sem_id %d\n", id);
		err = -1;
		goto err;
	}
	
	if((sems[slot].count -= count) < 0) {
		// we need to block
		struct thread *t = thread_get_current_thread();
		
		t->next_state = THREAD_STATE_WAITING;
		t->sem_count = count;
		t->blocked_sem_id = id;
		thread_enqueue(t, &sems[slot].q);
	
		if((flags & SEM_FLAG_TIMEOUT) != 0) {
//			dprintf("sem_acquire_etc: setting timeout sem for %d %d usecs, semid %d, tid %d\n",
//				timeout, sem_id, t->id);
			// set up an event to go off, with the thread struct as the data
			timer_set_event(timeout, TIMER_MODE_ONESHOT, &sem_timeout, (void *)t);
		}

		GRAB_THREAD_LOCK();
		RELEASE_SEM_LOCK(sems[slot]);
		thread_resched();
		RELEASE_THREAD_LOCK();
		int_restore_interrupts(state);
		return 0;		
	}
	
err:
	RELEASE_SEM_LOCK(sems[slot]);
	int_restore_interrupts(state);

	return err;
}

int sem_release(sem_id id, int count)
{
	return sem_release_etc(id, count, 0);
}

int sem_release_etc(sem_id id, int count, int flags)
{
	int slot = id % MAX_SEMS;
	int state;
	int released_threads = 0;
	int err = 0;
	struct thread *ready_threads[READY_THREAD_CACHE_SIZE];
	int ready_threads_count = 0;
	
	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		dprintf("sem_release_etc: invalid sem_id %d\n", id);
		err = -1;
		goto err;
	}

	while(count > 0) {
		int delta = count;
		if(sems[slot].count < 0) {
			struct thread *t = thread_lookat_queue(&sems[slot].q);
			
			delta = min(count, t->sem_count);
			t->sem_count -= delta;
			if(t->sem_count <= 0) {
				// release this thread
				t = thread_dequeue(&sems[slot].q);
				t->state = THREAD_STATE_READY;
				ready_threads[ready_threads_count++] = t;
				released_threads++;
			}
		}
		if(ready_threads_count == READY_THREAD_CACHE_SIZE) {
			// put a batch of em in the runq a once				
			GRAB_THREAD_LOCK();
			for(; ready_threads_count > 0; ready_threads_count--)
				thread_enqueue_run_q(ready_threads[ready_threads_count - 1]);
			RELEASE_THREAD_LOCK();
			// ready_threads_count is back to 0
		}
					
		sems[slot].count += delta;
		count -= delta;
	}
	if(released_threads > 0) {
		GRAB_THREAD_LOCK();
		// put any leftovers in the runq
		if(ready_threads_count > 0) {
			for(; ready_threads_count > 0; ready_threads_count--)
				thread_enqueue_run_q(ready_threads[ready_threads_count - 1]);
		}
		if((flags & SEM_FLAG_NO_RESCHED) == 0) {
			RELEASE_SEM_LOCK(sems[slot]);
			thread_resched();
		}
		RELEASE_THREAD_LOCK();
		goto outnolock;			
	}

err:
	RELEASE_SEM_LOCK(sems[slot]);
outnolock:
	int_restore_interrupts(state);

	return err;
}

