/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _SEM_H
#define _SEM_H

#include <kernel/thread.h>
#include <boot/stage2.h>

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2
#define SEM_FLAG_INTERRUPTABLE 4

int sem_init(kernel_args *ka);
sem_id sem_create(int count, const char *name);
int sem_delete(sem_id id);
int sem_delete_etc(sem_id id, int return_code);
int sem_acquire(sem_id id, int count);
int sem_acquire_etc(sem_id id, int count, int flags, time_t timeout, int *deleted_retcode);
int sem_release(sem_id id, int count);
int sem_release_etc(sem_id id, int count, int flags);

sem_id user_sem_create(int count, const char *name);
int user_sem_delete(sem_id id);
int user_sem_delete_etc(sem_id id, int return_code);
int user_sem_acquire(sem_id id, int count);
int user_sem_acquire_etc(sem_id id, int count, int flags, time_t timeout, int *deleted_retcode);
int user_sem_release(sem_id id, int count);
int user_sem_release_etc(sem_id id, int count, int flags);

int sem_delete_owned_sems(proc_id owner);
int sem_interrupt_thread(struct thread *t);

#endif

