#include <string.h>

#include "kernel.h"
#include "sem.h"
#include "spinlock.h"
#include "int.h"
#include "timer.h"
#include "debug.h"

static struct sem_entry *sems = NULL;
static int next_sem = 0;

struct sem_timeout {
	int sem_id;
	int thread_id;
};

int sem_init(struct kernel_args *ka)
{
	int i;
	TOUCH(ka);
	
	// create and initialize semaphore table
	sems = (struct sem_entry *)kmalloc(sizeof(struct sem_entry *) * MAX_SEMS);
	memset(sems, 0, sizeof(struct sem_entry *) * MAX_SEMS);
	for(i=0; i<MAX_SEMS; i++)
		sems[i].sem_id = -1;

	return 0;
}

int sem_create(int count, char *name)
{
	int i;
	int state;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	// find the first empty spot
	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].sem_id == -1) {
			// empty one
			if((next_sem % MAX_SEMS) < i) {
				// make the sem id be a multiple of the slot it's in
				next_sem += i - (next_sem % MAX_SEMS);
			}
			sems[i].sem_id = next_sem++;

			sems[i].count = count;
			sems[i].name = (char *)kmalloc(strlen(name)+1);
			if(sems[i].name == NULL) {
				sems[i].sem_id = -1;
				return -1;
			}
			strcpy(sems[i].name, name);

			RELEASE_THREAD_LOCK();
			int_restore_interrupts(state);

			return sems[i].sem_id;
		}
	}
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return -1;
}

int sem_delete(int sem_id)
{
	int slot = sem_id % MAX_SEMS;
	int state;
	int err = 0;
	struct thread *t;
	int released_threads = 0;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	if(sems[slot].sem_id != sem_id) {
		dprintf("sem_delete: invalid sem_id %d\n", sem_id);
		err = -1;
		goto err;
	}
	
	// free any threads waiting for this semaphore
	while((t = thread_dequeue(&sems[slot].q)) != NULL) {
		t->state = THREAD_STATE_READY;
		thread_enqueue_run_q(t);
		released_threads++;
	}

	kfree(sems[slot].name);
	sems[slot].sem_id = -1;		
	
	if(released_threads > 0)
		thread_resched();

err:
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return err;
}

// Called from a timer handler. Wakes up a semaphore
static void sem_timeout(void *data)
{
	struct sem_timeout *to = (struct sem_timeout *)data;
	int slot = to->sem_id % MAX_SEMS;
	int state;
	struct thread *t;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
//	dprintf("sem_timeout: called on 0x%x sem %d, tid %d\n", to, to->sem_id, to->thread_id);
		
	t = thread_dequeue_id(&sems[slot].q, to->thread_id);
	if(t != NULL) {
		t->state = THREAD_STATE_READY;
		thread_enqueue_run_q(t);
	}

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	kfree(to);
}

int sem_acquire(int sem_id, int count)
{
	return sem_acquire_etc(sem_id, count, 0, 0);
}

int sem_acquire_etc(int sem_id, int count, int flags, long long timeout)
{
	int slot = sem_id % MAX_SEMS;
	int state;
	int err = 0;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	if(sems[slot].sem_id != sem_id) {
		dprintf("sem_acquire_etc: invalid sem_id %d\n", sem_id);
		err = -1;
		goto err;
	}
	
	if((sems[slot].count -= count) < 0) {
		// we need to block
		struct thread *t = thread_get_current_thread();
		
		t->next_state = THREAD_STATE_WAITING;
		t->sem_count = count;
		thread_enqueue(t, &sems[slot].q);
	
		if((flags & SEM_FLAG_TIMEOUT) != 0) {
			struct sem_timeout *to;
			
			to = kmalloc(sizeof(struct sem_timeout));
			if(to == NULL) {
				sems[slot].count += count;
				err = -1; // ENOMEM
				goto err;
			}
			to->sem_id = sem_id;
			to->thread_id = t->id;
			
//			dprintf("sem_acquire_etc: setting timeout sem for %d %d usecs, semid %d, tid %d, 0x%x\n",
//				timeout, sem_id, t->id, to);
			timer_set_event(timeout, TIMER_MODE_ONESHOT, &sem_timeout, to);
		}
	
		thread_resched();
	}
	
err:
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return err;
}

int sem_release(int sem_id, int count)
{
	return sem_release_etc(sem_id, count, 0);
}

int sem_release_etc(int sem_id, int count, int flags)
{
	int slot = sem_id % MAX_SEMS;
	int state;
	int released_threads = 0;
	int err = 0;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	if(sems[slot].sem_id != sem_id) {
		dprintf("sem_release_etc: invalid sem_id %d\n", sem_id);
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
				thread_enqueue_run_q(t);
				released_threads++;
			}
		}
		
		sems[slot].count += delta;
		count -= delta;
	}

	if(released_threads > 0 && (flags & SEM_FLAG_NO_RESCHED) == 0)
		thread_resched();

err:
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return err;
}

