/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _LIBSYS_SYSCALLS_H
#define _LIBSYS_SYSCALLS_H

#include <types.h>
#include <kernel/ktypes.h>

typedef enum {
	STREAM_TYPE_NULL = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
} stream_type;

typedef enum {
	SEEK_SET = 0,
	SEEK_CUR,
	SEEK_END
} seek_type;

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2

int sys_null();
int sys_open(const char *path, const char *stream, stream_type stream_type);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_read(int fd, void *buf, off_t pos, size_t *len);
int sys_write(int fd, const void *buf, off_t pos, size_t *len);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_close(int fd);
int sys_create(const char *path, const char *stream, stream_type stream_type);
time_t sys_system_time();
int sys_snooze(time_t time);
sem_id sys_sem_create(int count, const char *name);
int sys_sem_delete(sem_id id);
int sys_sem_acquire(sem_id id, int count);
int sys_sem_acquire_etc(sem_id id, int count, int flags, time_t timeout);
int sys_sem_release(sem_id id, int count);
int sys_sem_release_etc(sem_id id, int count, int flags);
thread_id sys_get_current_thread_id();
void sys_exit(int retcode);
proc_id sys_proc_create_proc(const char *path, const char *name, int priority);
int sys_thread_wait_on_thread(thread_id tid, int *retcode);
int sys_proc_wait_on_proc(proc_id pid, int *retcode);

#endif

