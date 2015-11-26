/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_SEM_H
#define _KERNEL_SEM_H

#include <kernel/thread.h>
#include <boot/stage2.h>

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2
#define SEM_FLAG_INTERRUPTABLE 4

struct sem_info {
    sem_id      sem;
    proc_id     proc;
    char        name[SYS_MAX_OS_NAME_LEN];
    int32       count;
    thread_id   latest_holder;
};

int sem_init(kernel_args *ka);
sem_id sem_create_etc(int count, const char *name, proc_id owner);
sem_id sem_create(int count, const char *name);
int sem_delete(sem_id id);
int sem_delete_etc(sem_id id, int return_code);
int sem_acquire(sem_id id, int count);
int sem_acquire_etc(sem_id id, int count, int flags, bigtime_t timeout, int *deleted_retcode);
int sem_release(sem_id id, int count);
int sem_release_etc(sem_id id, int count, int flags);
int sem_get_count(sem_id id, int32* thread_count);
int sem_get_sem_info(sem_id id, struct sem_info *info);
int sem_get_next_sem_info(proc_id proc, uint32 *cookie, struct sem_info *info);
int set_sem_owner(sem_id id, proc_id proc);

sem_id user_sem_create(int count, const char *name);
int user_sem_delete(sem_id id);
int user_sem_delete_etc(sem_id id, int return_code);
int user_sem_acquire(sem_id id, int count);
int user_sem_acquire_etc(sem_id id, int count, int flags, bigtime_t timeout, int *deleted_retcode);
int user_sem_release(sem_id id, int count);
int user_sem_release_etc(sem_id id, int count, int flags);
int user_sem_get_count(sem_id id, int32* thread_count);
int user_sem_get_sem_info(sem_id id, struct sem_info *info);
int user_sem_get_next_sem_info(proc_id proc, uint32 *cookie, struct sem_info *info);
int user_set_sem_owner(sem_id id, proc_id proc);

int sem_delete_owned_sems(proc_id owner);
int sem_interrupt_thread(struct thread *t);

#endif

