/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/sem.h>
#include <kernel/lock.h>
#include <kernel/debug.h>

int recursive_lock_create(recursive_lock *lock)
{
	if(lock == NULL)
		return -1;
	lock->holder = 0;
	lock->recursion = 0;
	lock->sem = sem_create(1, "recursive_lock_sem");
//	if(lock->sem < 0)
//		return -1;
	return 0;
}

void recursive_lock_destroy(recursive_lock *lock)
{
	if(lock == NULL)
		return;
	if(lock->sem > 0)
		sem_delete(lock->sem);
	lock->sem = -1;
}

void recursive_lock_lock(recursive_lock *lock)
{
	thread_id thid = thread_get_current_thread_id();
	
	if(thid != lock->holder) {
		sem_acquire(lock->sem, 1);
		
		lock->holder = thid;
	}
	lock->recursion++;
}

void recursive_lock_unlock(recursive_lock *lock)
{
	thread_id thid = thread_get_current_thread_id();

	if(thid != lock->holder)
		panic("recursive_lock 0x%x unlocked by non-holder thread!\n", lock);
	
	if(--lock->recursion == 0) {
		lock->holder = 0;
		sem_release(lock->sem, 1);
	}
}
