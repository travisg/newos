/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _LOCK_H
#define _LOCK_H

#include <kernel/kernel.h>

typedef struct recursive_lock {
	thread_id holder;
	int recursion;
	sem_id sem;
} recursive_lock;

int recursive_lock_create(recursive_lock *lock);
void recursive_lock_destroy(recursive_lock *lock);
void recursive_lock_lock(recursive_lock *lock);
void recursive_lock_unlock(recursive_lock *lock);

#endif

