/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/sem.h>
#include <kernel/lock.h>
#include <kernel/debug.h>
#include <kernel/arch/cpu.h>
#include <sys/errors.h>

int recursive_lock_create(recursive_lock *lock)
{
	if(lock == NULL)
		return ERR_INVALID_ARGS;
	lock->holder = -1;
	lock->recursion = 0;
	lock->sem = sem_create(1, "recursive_lock_sem");
//	if(lock->sem < 0)
//		return -1;
	return NO_ERROR;
}

void recursive_lock_destroy(recursive_lock *lock)
{
	if(lock == NULL)
		return;
	if(lock->sem > 0)
		sem_delete(lock->sem);
	lock->sem = -1;
}

bool recursive_lock_lock(recursive_lock *lock)
{
	thread_id thid = thread_get_current_thread_id();
	bool retval = false;
	
	if(thid != lock->holder) {
		sem_acquire(lock->sem, 1);
		
		lock->holder = thid;
		retval = true;
	}
	lock->recursion++;
	return retval;
}

bool recursive_lock_unlock(recursive_lock *lock)
{
	thread_id thid = thread_get_current_thread_id();
	bool retval = false;

	if(thid != lock->holder)
		panic("recursive_lock 0x%x unlocked by non-holder thread!\n", lock);
	
	if(--lock->recursion == 0) {
		lock->holder = -1;
		sem_release(lock->sem, 1);
		retval = true;
	}
	return retval;
}

int mutex_init(mutex *m, const char *in_name)
{
	const char *name;

	if(m == NULL)
		return ERR_INVALID_ARGS;

	if(in_name == NULL)
		name = "mutex_sem";
	else
		name = in_name;

	m->count = 0;

	m->sem = sem_create(0, name);
	if(m->sem < 0)
		return m->sem;

	return 0;
}

void mutex_destroy(mutex *m)
{
	if(m == NULL)
		return;

	if(m->sem >= 0) {
		sem_delete(m->sem);
	}
}

void mutex_lock(mutex *m)
{
	if(atomic_add(&m->count, 1) >= 1)
		sem_acquire(m->sem, 1);
}

void mutex_unlock(mutex *m)
{
	if(atomic_add(&m->count, -1) > 1)
		sem_release(m->sem, 1);
}

