#include <string.h>

#include "kernel.h"
#include "sem.h"
#include "spinlock.h"
#include "int.h"

static struct sem_entry *sems = NULL;
static int next_sem = 0;

int sem_init(struct kernel_args *ka)
{
	int i;
	
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
	GRAB_THREAD_LOCK;
	
	// find the first empty spot
	for(i=0; i<MAX_SEMS; i++) {
		if(sems[i].sem_id == -1) {
			// empty one
			if((next_sem % MAX_SEMS) < i) {
				// make the sem id be a multiple of the slot it's in
				next_sem += i - (next_sem % MAX_SEMS);
			}
			sems[i].sem_id = next_sem++;
			RELEASE_THREAD_LOCK;
			int_restore_interrupts(state);
			sems[i].count = count;
			sems[i].name = (char *)kmalloc(strlen(name)+1);
			strcpy(sems[i].name, name);
			return sems[i].sem_id;
		}
	}
	RELEASE_THREAD_LOCK;
	int_restore_interrupts(state);

	return -1;
}

int sem_acquire(int sem_id, int count)
{
	return sem_acquire_etc(sem_id, count, 0);
}

int sem_acquire_etc(int sem_id, int count, int flags)
{
	int slot = sem_id & MAX_SEMS;
	int state;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK;
	
	if((sems[slot].count -= count) < 0) {
		// we need to block
		struct thread *t = thread_get_current_thread();
		
		t->next_state = THREAD_STATE_WAITING;
		t->sem_count = count;
		thread_enqueue(t, &sems[slot].q);
	
		thread_resched();
	}
	
	RELEASE_THREAD_LOCK;
	int_restore_interrupts(state);

	return 0;
}

int sem_release(int sem_id, int count)
{
	return sem_release_etc(sem_id, count, 0);
}

int sem_release_etc(int sem_id, int count, int flags)
{
	int slot = sem_id & MAX_SEMS;
	int state;
	int released_threads = 0;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK;

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

	RELEASE_THREAD_LOCK;
	int_restore_interrupts(state);

	return 0;
}

