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
bool recursive_lock_lock(recursive_lock *lock);
bool recursive_lock_unlock(recursive_lock *lock);

typedef struct mutex {
	int count;
	sem_id sem;
} mutex;

int mutex_init(mutex *m, const char *name);
void mutex_destroy(mutex *m);
void mutex_lock(mutex *m);
void mutex_unlock(mutex *m);

#endif

