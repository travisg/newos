/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/sem.h>
#include <kernel/smp.h>
#include <kernel/int.h>
#include <kernel/timer.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/thread.h>
#include <sys/errors.h>

#include <boot/stage2.h>

#include <libc/string.h>

static struct sem_entry *sems = NULL;
static region_id sem_region = 0;
static bool sems_active = false;

static sem_id next_sem = 0;

static const char *temp_sem_name = "temp sem";

static int sem_spinlock = 0;
#define GRAB_SEM_LIST_LOCK() acquire_spinlock(&sem_spinlock)
#define RELEASE_SEM_LIST_LOCK() release_spinlock(&sem_spinlock)
#define GRAB_SEM_LOCK(s) acquire_spinlock(&(s).lock)
#define RELEASE_SEM_LOCK(s) release_spinlock(&(s).lock)

// used in functions that may put a bunch of threads in the run q at once
#define READY_THREAD_CACHE_SIZE 16

struct sem_timeout_args {
	thread_id blocked_thread;
	sem_id blocked_sem_id;
	int sem_count;
};

void dump_sem_list(int argc, char **argv)
{
	int i;

	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].id >= 0) {
			dprintf("0x%x\tid: 0x%x\t\tname: '%s'\n", &sems[i], sems[i].id, sems[i].name);
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

		if(num > KERNEL_BASE && num <= (KERNEL_BASE + (KERNEL_SIZE - 1))) {
			// XXX semi-hack
			_dump_sem_info((struct sem_entry *)num);
			return;
		} else {
			int slot = num % MAX_SEMS;
			if(sems[slot].id != (int)num) {
				dprintf("sem 0x%x doesn't exist!\n", num);
				return;
			}
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

	dprintf("sem_init: entry\n");

	// create and initialize semaphore table
	sem_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "sem_table", (void **)&sems,
		REGION_ADDR_ANY_ADDRESS, sizeof(struct sem_entry) * MAX_SEMS, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
	if(sem_region < 0) {
		panic("unable to allocate semaphore table!\n");
	}

	dprintf("memsetting len %d @ 0x%x\n", sizeof(struct sem_entry) * MAX_SEMS, sems);
	memset(sems, 0, sizeof(struct sem_entry) * MAX_SEMS);
	dprintf("done\n");
	for(i=0; i<MAX_SEMS; i++)
		sems[i].id = -1;

	sems_active = true;

	// add debugger commands
	dbg_add_command(&dump_sem_list, "sems", "Dump a list of all active semaphores");
	dbg_add_command(&dump_sem_info, "sem", "Dump info about a particular semaphore");

	dprintf("sem_init: exit\n");

	return 0;
}

sem_id sem_create(int count, const char *name)
{
	int i;
	int state;
	sem_id retval = ERR_SEM_OUT_OF_SLOTS;
	char *temp_name;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;

	temp_name = (char *)kmalloc(strlen(name)+1);
	if(temp_name == NULL)
		return ERR_NO_MEMORY;
	strcpy(temp_name, name);

	state = int_disable_interrupts();
	GRAB_SEM_LIST_LOCK();

	// find the first empty spot
	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].id == -1) {
			// make the sem id be a multiple of the slot it's in
			if(i >= next_sem % MAX_SEMS) {
				next_sem += i - next_sem % MAX_SEMS;
			} else {
				next_sem += MAX_SEMS - (next_sem % MAX_SEMS - i);
			}
			sems[i].id = next_sem++;

			sems[i].lock = 0;
			GRAB_SEM_LOCK(sems[i]);
			RELEASE_SEM_LIST_LOCK();

			sems[i].q.tail = NULL;
			sems[i].q.head = NULL;
			sems[i].count = count;
			sems[i].name = temp_name;
			retval = sems[i].id;

			RELEASE_SEM_LOCK(sems[i]);
			break;
		}
	}
	if(i >= MAX_SEMS)
		RELEASE_SEM_LIST_LOCK();

	if(retval < 0) {
		kfree(temp_name);
	}

	int_restore_interrupts(state);

	return retval;
}

int sem_delete(sem_id id)
{
	return sem_delete_etc(id, 0);
}

int sem_delete_etc(sem_id id, int return_code)
{
	int slot = id % MAX_SEMS;
	int state;
	int err = NO_ERROR;
	struct thread *t;
	int released_threads = 0;
	char *old_name;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		RELEASE_SEM_LOCK(sems[slot]);
		int_restore_interrupts(state);
		dprintf("sem_delete: invalid sem_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	// free any threads waiting for this semaphore
	// put them in the runq in batches, to keep the amount of time
	// spent with the thread lock held down to a minimum
	{
		struct thread *ready_threads[READY_THREAD_CACHE_SIZE];
		int ready_threads_count = 0;

		while((t = thread_dequeue(&sems[slot].q)) != NULL) {
			t->state = THREAD_STATE_READY;
			t->sem_deleted_retcode = return_code;
			t->sem_count = 0;
			ready_threads[ready_threads_count++] = t;
			released_threads++;

			if(ready_threads_count == READY_THREAD_CACHE_SIZE) {
				// put a batch of em in the runq at once
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
static int sem_timeout(void *data)
{
	struct sem_timeout_args *args = (struct sem_timeout_args *)data;
	struct thread *t;
	int slot;
	int state;

	t = thread_get_thread_struct(args->blocked_thread);
	if(t == NULL)
		return INT_NO_RESCHEDULE;
	slot = args->blocked_sem_id % MAX_SEMS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

//	dprintf("sem_timeout: called on 0x%x sem %d, tid %d\n", to, to->sem_id, to->thread_id);

	if(sems[slot].id != args->blocked_sem_id) {
		// this thread was not waiting on this semaphore
		panic("sem_timeout: thid %d was trying to wait on sem %d which doesn't exist!\n",
			args->blocked_thread, args->blocked_sem_id);
	}

	t = thread_dequeue_id(&sems[slot].q, t->id);
	if(t != NULL) {
		sems[slot].count += args->sem_count;
		t->state = THREAD_STATE_READY;
		GRAB_THREAD_LOCK();
		thread_enqueue_run_q(t);
		RELEASE_THREAD_LOCK();
	}

	// XXX handle possibly releasing more threads here

	RELEASE_SEM_LOCK(sems[slot]);
	int_restore_interrupts(state);

	return INT_RESCHEDULE;
}

int sem_acquire(sem_id id, int count)
{
	return sem_acquire_etc(id, count, 0, 0, NULL);
}

int sem_acquire_etc(sem_id id, int count, int flags, time_t timeout, int *deleted_retcode)
{
	int slot = id % MAX_SEMS;
	int state;
	int err = 0;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;

	if(id < 0)
		return ERR_INVALID_HANDLE;

	if(count <= 0)
		return ERR_INVALID_ARGS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		dprintf("sem_acquire_etc: invalid sem_id %d\n", id);
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	if(sems[slot].count - count < 0 && (flags & SEM_FLAG_TIMEOUT) != 0 && timeout == 0) {
		// immediate timeout
		err = ERR_SEM_TIMED_OUT;
		goto err;
	}

	if((sems[slot].count -= count) < 0) {
		// we need to block
		struct thread *t = thread_get_current_thread();
		struct timer_event timer; // stick it on the heap, since we may be blocking here
		struct sem_timeout_args args;

		t->next_state = THREAD_STATE_WAITING;
		t->sem_count = min(-sems[slot].count, count); // store the count we need to restore upon release
		t->sem_deleted_retcode = 0;
		thread_enqueue(t, &sems[slot].q);

		if((flags & SEM_FLAG_TIMEOUT) != 0) {
//			dprintf("sem_acquire_etc: setting timeout sem for %d %d usecs, semid %d, tid %d\n",
//				timeout, sem_id, t->id);
			// set up an event to go off, with the thread struct as the data
			args.blocked_sem_id = id;
			args.blocked_thread = t->id;
			args.sem_count = count;
			timer_setup_timer(&sem_timeout, &args, &timer);
			timer_set_event(timeout, TIMER_MODE_ONESHOT, &timer);
		}

		RELEASE_SEM_LOCK(sems[slot]);
		GRAB_THREAD_LOCK();
		thread_resched();
		RELEASE_THREAD_LOCK();

		if((flags & SEM_FLAG_TIMEOUT) != 0) {
			// cancel the timer event, in case the sem was deleted and a timer in still active
			timer_cancel_event(&timer);
		}

		int_restore_interrupts(state);
		if(deleted_retcode != NULL)
			*deleted_retcode = t->sem_deleted_retcode;
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

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;

	if(id < 0)
		return ERR_INVALID_HANDLE;

	if(count <= 0)
		return ERR_INVALID_ARGS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		dprintf("sem_release_etc: invalid sem_id %d\n", id);
		err = ERR_INVALID_HANDLE;
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
				t->sem_count = 0;
				t->sem_deleted_retcode = 0;
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
	RELEASE_SEM_LOCK(sems[slot]);
	if(released_threads > 0) {
		// put any leftovers in the runq
		GRAB_THREAD_LOCK();
		if(ready_threads_count > 0) {
			for(; ready_threads_count > 0; ready_threads_count--)
				thread_enqueue_run_q(ready_threads[ready_threads_count - 1]);
		}
		if((flags & SEM_FLAG_NO_RESCHED) == 0) {
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

