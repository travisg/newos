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

struct sem_entry {
	sem_id    id;
	int       count;
	struct thread_queue q;
	char      *name;
	int       lock;
	proc_id   owner;
};

#define MAX_SEMS 4096

static struct sem_entry *sems = NULL;
static region_id sem_region = 0;
static bool sems_active = false;

static sem_id next_sem = 0;

static int sem_spinlock = 0;
#define GRAB_SEM_LIST_LOCK() acquire_spinlock(&sem_spinlock)
#define RELEASE_SEM_LIST_LOCK() release_spinlock(&sem_spinlock)
#define GRAB_SEM_LOCK(s) acquire_spinlock(&(s).lock)
#define RELEASE_SEM_LOCK(s) release_spinlock(&(s).lock)

// used in functions that may put a bunch of threads in the run q at once
#define READY_THREAD_CACHE_SIZE 16

static int remove_thread_from_sem(struct thread *t, struct sem_entry *sem, struct thread_queue *queue, int sem_errcode);

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
	dprintf("owner: 0x%x\n", sem->owner);
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

	memset(sems, 0, sizeof(struct sem_entry) * MAX_SEMS);
	for(i=0; i<MAX_SEMS; i++)
		sems[i].id = -1;

	// add debugger commands
	dbg_add_command(&dump_sem_list, "sems", "Dump a list of all active semaphores");
	dbg_add_command(&dump_sem_info, "sem", "Dump info about a particular semaphore");

	dprintf("sem_init: exit\n");

	sems_active = true;

	return 0;
}

static sem_id sem_create_etc(int count, const char *name, proc_id owner)
{
	int i;
	int state;
	sem_id retval = ERR_SEM_OUT_OF_SLOTS;
	char *temp_name;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;

	if(name) {
		int name_len = strlen(name);

		temp_name = (char *)kmalloc(min(name_len + 1, SYS_MAX_OS_NAME_LEN));
		if(temp_name == NULL)
			return ERR_NO_MEMORY;
		strncpy(temp_name, name, SYS_MAX_OS_NAME_LEN-1);
		temp_name[SYS_MAX_OS_NAME_LEN-1] = 0;
	} else {
		temp_name = (char *)kmalloc(sizeof("default_sem_name")+1);
		if(temp_name == NULL)
			return ERR_NO_MEMORY;
		strcpy(temp_name, "default_sem_name");
	}

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
			sems[i].owner = owner;
			retval = sems[i].id;

			RELEASE_SEM_LOCK(sems[i]);
			goto out;
		}
	}

err:
	RELEASE_SEM_LIST_LOCK();
	kfree(temp_name);

out:
	int_restore_interrupts(state);

	return retval;
}

sem_id sem_create(int count, const char *name)
{
	return sem_create_etc(count, name, proc_get_kernel_proc_id());
}

int sem_delete(sem_id id)
{
	return sem_delete_etc(id, 0);
}

int sem_delete_etc(sem_id id, int return_code)
{
	int slot;
	int state;
	int err = NO_ERROR;
	struct thread *t;
	int released_threads;
	char *old_name;
	struct thread_queue release_queue;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_SEMS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		RELEASE_SEM_LOCK(sems[slot]);
		int_restore_interrupts(state);
		dprintf("sem_delete: invalid sem_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	released_threads = 0;
	release_queue.head = release_queue.tail = NULL;

	// free any threads waiting for this semaphore
	while((t = thread_dequeue(&sems[slot].q)) != NULL) {
		t->state = THREAD_STATE_READY;
		t->sem_errcode = ERR_SEM_DELETED;
		t->sem_deleted_retcode = return_code;
		t->sem_count = 0;
		thread_enqueue(t, &release_queue);
		released_threads++;
	}

	sems[slot].id = -1;
	old_name = sems[slot].name;
	sems[slot].name = NULL;

	RELEASE_SEM_LOCK(sems[slot]);

	if(released_threads > 0) {
		GRAB_THREAD_LOCK();
		while((t = thread_dequeue(&release_queue)) != NULL) {
			thread_enqueue_run_q(t);
		}
		thread_resched();
		RELEASE_THREAD_LOCK();
	}

	int_restore_interrupts(state);

	kfree(old_name);

	return err;
}

// Called from a timer handler. Wakes up a semaphore
static int sem_timeout(void *data)
{
	struct sem_timeout_args *args = (struct sem_timeout_args *)data;
	struct thread *t;
	int slot;
	int state;
	struct thread_queue wakeup_queue;

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

	wakeup_queue.head = wakeup_queue.tail = NULL;
	remove_thread_from_sem(t, &sems[slot], &wakeup_queue, ERR_SEM_TIMED_OUT);

	RELEASE_SEM_LOCK(sems[slot]);

	GRAB_THREAD_LOCK();
	// put the threads in the run q here to make sure we dont deadlock in sem_interrupt_thread
	while((t = thread_dequeue(&wakeup_queue)) != NULL) {
		thread_enqueue_run_q(t);
	}
	RELEASE_THREAD_LOCK();

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

	if(id < 0) {
		dprintf("sem_acquire_etc: invalid sem handle %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	if(count <= 0)
		return ERR_INVALID_ARGS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		dprintf("sem_acquire_etc: bad sem_id %d\n", id);
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	if(sems[slot].count - count < 0 && (flags & SEM_FLAG_TIMEOUT) != 0 && timeout <= 0) {
		// immediate timeout
		err = ERR_SEM_TIMED_OUT;
		goto err;
	}

	if((sems[slot].count -= count) < 0) {
		// we need to block
		struct thread *t = thread_get_current_thread();
		struct timer_event timer; // stick it on the heap, since we may be blocking here
		struct sem_timeout_args args;

		// do a quick check to see if the thread has any pending kill signals
		// this should catch most of the cases where the thread had a signal
		if((flags & SEM_FLAG_INTERRUPTABLE) && (t->pending_signals & SIG_KILL)) {
			sems[slot].count += count;
			err = ERR_SEM_INTERRUPTED;
			goto err;
		}

		t->next_state = THREAD_STATE_WAITING;
		t->sem_flags = flags;
		t->sem_blocking = id;
		t->sem_acquire_count = count;
		t->sem_count = min(-sems[slot].count, count); // store the count we need to restore upon release
		t->sem_deleted_retcode = 0;
		t->sem_errcode = NO_ERROR;
		thread_enqueue(t, &sems[slot].q);

		if((flags & SEM_FLAG_TIMEOUT) != 0) {
//			dprintf("sem_acquire_etc: setting timeout sem for %d %d usecs, semid %d, tid %d\n",
//				timeout, sem_id, t->id);
			// set up an event to go off with the thread struct as the data
			args.blocked_sem_id = id;
			args.blocked_thread = t->id;
			args.sem_count = count;
			timer_setup_timer(&sem_timeout, &args, &timer);
			timer_set_event(timeout, TIMER_MODE_ONESHOT, &timer);
		}

		RELEASE_SEM_LOCK(sems[slot]);
		GRAB_THREAD_LOCK();
		// check again to see if a kill signal is pending.
		// it may have been delivered while setting up the sem, though it's pretty unlikely
		if((flags & SEM_FLAG_INTERRUPTABLE) && (t->pending_signals & SIG_KILL)) {
			struct thread_queue wakeup_queue;
			// ok, so a tiny race happened where a signal was delivered to this thread while
			// it was setting up the sem. We can only be sure a signal wasn't delivered
			// here, since the threadlock is held. The previous check would have found most
			// instances, but there was a race, so we have to handle it. It'll be more messy...
			wakeup_queue.head = wakeup_queue.tail = NULL;
			GRAB_SEM_LOCK(sems[slot]);
			if(sems[slot].id == id) {
				remove_thread_from_sem(t, &sems[slot], &wakeup_queue, ERR_SEM_INTERRUPTED);
			}
			RELEASE_SEM_LOCK(sems[slot]);
			while((t = thread_dequeue(&wakeup_queue)) != NULL) {
				thread_enqueue_run_q(t);
			}
			// fall through and reschedule since another thread with a higher priority may have been woken up
		}
		thread_resched();
		RELEASE_THREAD_LOCK();

		if((flags & SEM_FLAG_TIMEOUT) != 0) {
			if(t->sem_errcode < 0 && t->sem_errcode != ERR_SEM_TIMED_OUT) {
				// cancel the timer event, the sem may have been deleted or interrupted
				// with the timer still active
				timer_cancel_event(&timer);
			}
		}

		int_restore_interrupts(state);
		if(deleted_retcode != NULL)
			*deleted_retcode = t->sem_deleted_retcode;
		return t->sem_errcode;
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
	struct thread_queue release_queue;

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

	// clear out a queue we will use to hold all of the threads that we will have to
	// put back into the run list. This is done so the thread lock wont be held
	// while this sems lock is held since the two locks are grabbed in the other
	// order in sem_interrupt_thread.
	release_queue.head = release_queue.tail = NULL;

	while(count > 0) {
		int delta = count;
		if(sems[slot].count < 0) {
			struct thread *t = thread_lookat_queue(&sems[slot].q);

			delta = min(count, t->sem_count);
			t->sem_count -= delta;
			if(t->sem_count <= 0) {
				// release this thread
				t = thread_dequeue(&sems[slot].q);
				thread_enqueue(t, &release_queue);
				t->state = THREAD_STATE_READY;
				released_threads++;
				t->sem_count = 0;
				t->sem_deleted_retcode = 0;
			}
		}

		sems[slot].count += delta;
		count -= delta;
	}
	RELEASE_SEM_LOCK(sems[slot]);

	// pull off any items in the release queue and put them in the run queue
	if(released_threads > 0) {
		struct thread *t;
		GRAB_THREAD_LOCK();
		while((t = thread_dequeue(&release_queue)) != NULL) {
			thread_enqueue_run_q(t);
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

int
sem_get_count(sem_id id)
{
	int slot;
	int state;
	int count;

	if(sems_active == false)
		return ERR_SEM_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_SEMS;

	state = int_disable_interrupts();
	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != id) {
		RELEASE_SEM_LOCK(sems[slot]);
		int_restore_interrupts(state);
		dprintf("sem_get_count: invalid sem_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}
	
	count = sems[slot].count;

	RELEASE_SEM_LOCK(sems[slot]);
	int_restore_interrupts(state);

	return count;
}


// Wake up a thread that's blocked on a semaphore
// this function must be entered with interrupts disabled and THREADLOCK held
int sem_interrupt_thread(struct thread *t)
{
	struct thread *t1;
	int slot;
	int state;
	struct thread_queue wakeup_queue;

	dprintf("sem_interrupt_thread: called on thread 0x%x (%d), blocked on sem 0x%x\n", t, t->id, t->sem_blocking);

	if(t->state != THREAD_STATE_WAITING)
		return ERR_INVALID_ARGS;
	if(t->sem_blocking < 0)
		return ERR_INVALID_ARGS;
	if((t->sem_flags & SEM_FLAG_INTERRUPTABLE) == 0)
		return ERR_SEM_NOT_INTERRUPTABLE;

	slot = t->sem_blocking % MAX_SEMS;

	GRAB_SEM_LOCK(sems[slot]);

	if(sems[slot].id != t->sem_blocking) {
		panic("sem_interrupt_thread: thread 0x%x sez it's blocking on sem 0x%x, but that sem doesn't exist!\n", t->id, t->sem_blocking);
	}

	wakeup_queue.head = wakeup_queue.tail = NULL;
	if(remove_thread_from_sem(t, &sems[slot], &wakeup_queue, ERR_SEM_INTERRUPTED) == ERR_NOT_FOUND)
		panic("sem_interrupt_thread: thread 0x%x not found in sem 0x%x's wait queue\n", t->id, t->sem_blocking);

	RELEASE_SEM_LOCK(sems[slot]);

	while((t = thread_dequeue(&wakeup_queue)) != NULL) {
		thread_enqueue_run_q(t);
	}

	return NO_ERROR;
}

// forcibly removes a thread from a semaphores wait q. May have to wake up other threads in the
// process. All threads that need to be woken up are added to the passed in thread_queue.
// must be called with sem lock held
static int remove_thread_from_sem(struct thread *t, struct sem_entry *sem, struct thread_queue *queue, int sem_errcode)
{
	struct thread *t1;

	// remove the thread from the queue and place it in the supplied queue
	t1 = thread_dequeue_id(&sem->q, t->id);
	if(t != t1)
		return ERR_NOT_FOUND;
	sem->count += t->sem_acquire_count;
	t->state = THREAD_STATE_READY;
	t->sem_errcode = sem_errcode;
	thread_enqueue(t, queue);

	// now see if more threads need to be woken up
	while(sem->count > 0 && (t1 = thread_lookat_queue(&sem->q))) {
		int delta = min(t->sem_count, sem->count);

		t->sem_count -= delta;
		if(t->sem_count <= 0) {
			t = thread_dequeue(&sem->q);
			t->state = THREAD_STATE_READY;
			thread_enqueue(t, queue);
		}
		sem->count -= delta;
	}
	return NO_ERROR;
}

/* this function cycles through the sem table, deleting all the sems that are owned by
   the passed proc_id */
int sem_delete_owned_sems(proc_id owner)
{
	int state;
	int i;
	int count = 0;

	state = int_disable_interrupts();
	GRAB_SEM_LIST_LOCK();

	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].id != -1 && sems[i].owner == owner) {
			sem_id id = sems[i].id;

			RELEASE_SEM_LIST_LOCK();
			int_restore_interrupts(state);

			sem_delete_etc(id, 0);
			count++;

			state = int_disable_interrupts();
			GRAB_SEM_LIST_LOCK();
		}
	}

	RELEASE_SEM_LIST_LOCK();
	int_restore_interrupts(state);

	return count;
}

sem_id user_sem_create(int count, const char *uname)
{
	if(uname != NULL) {
		char name[SYS_MAX_OS_NAME_LEN];
		int rc;

		if((addr)uname >= KERNEL_BASE && (addr)uname <= KERNEL_TOP)
			return ERR_VM_BAD_USER_MEMORY;

		rc = user_strncpy(name, uname, SYS_MAX_OS_NAME_LEN-1);
		if(rc < 0)
			return rc;
		name[SYS_MAX_OS_NAME_LEN-1] = 0;

		return sem_create_etc(count, name, proc_get_current_proc_id());
	} else {
		return sem_create_etc(count, NULL, proc_get_current_proc_id());
	}
}

int user_sem_delete(sem_id id)
{
	return sem_delete(id);
}

int user_sem_delete_etc(sem_id id, int return_code)
{
	return sem_delete_etc(id, return_code);
}

int user_sem_acquire(sem_id id, int count)
{
	return sem_acquire(id, count);
}

int user_sem_acquire_etc(sem_id id, int count, int flags, time_t timeout, int *deleted_retcode)
{
	if(deleted_retcode != NULL && ((addr)deleted_retcode >= KERNEL_BASE && (addr)deleted_retcode <= KERNEL_TOP))
		return ERR_VM_BAD_USER_MEMORY;

	if(deleted_retcode == NULL) {
		return sem_acquire_etc(id, count, flags, timeout, deleted_retcode);
	} else {
		int uretcode;
		int rc, rc2;

		rc = user_memcpy(&uretcode, deleted_retcode, sizeof(uretcode));
		if(rc < 0)
			return rc;

		rc = sem_acquire_etc(id, count, flags, timeout, &uretcode);
		if(rc < 0)
			return rc;

		rc2 = user_memcpy(deleted_retcode, &uretcode, sizeof(uretcode));
		if(rc2 < 0)
			return rc2;

		return rc;
	}
}

int user_sem_release(sem_id id, int count)
{
	return sem_release(id, count);
}

int user_sem_release_etc(sem_id id, int count, int flags)
{
	return sem_release_etc(id, count, flags);
}

