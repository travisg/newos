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

struct vnode_stat {
	off_t size;
};

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2

// info about a region that external entities may want to know
typedef struct vm_region_info {
	region_id id;
	addr base;
	addr size;
	int lock;
	int wiring;
} vm_region_info;

// args for the create_area funcs
enum {
	REGION_ADDR_ANY_ADDRESS = 0,
	REGION_ADDR_EXACT_ADDRESS
};

enum {
	REGION_WIRING_LAZY = 0,
	REGION_WIRING_WIRED,
	REGION_WIRING_WIRED_ALREADY,
	REGION_WIRING_WIRED_CONTIG,
	REGION_WIRING_WIRED_SPECIAL
};

enum {
	PHYSICAL_PAGE_NO_WAIT = 0,
	PHYSICAL_PAGE_CAN_WAIT,
};

#define LOCK_RO        0x0
#define LOCK_RW        0x1
#define LOCK_KERNEL    0x2
#define LOCK_MASK      0x3

int sys_null();
int sys_open(const char *path, const char *stream, stream_type stream_type);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_read(int fd, void *buf, off_t pos, size_t *len);
int sys_write(int fd, const void *buf, off_t pos, size_t *len);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_close(int fd);
int sys_create(const char *path, const char *stream, stream_type stream_type);
int sys_stat(const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat);
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
thread_id sys_thread_create_thread(const char *name, int priority, addr entry);
int sys_thread_wait_on_thread(thread_id tid, int *retcode);
int sys_thread_suspend_thread(thread_id tid);
int sys_thread_resume_thread(thread_id tid);
int sys_thread_kill_thread(thread_id tid);
int sys_proc_kill_proc(proc_id pid);
proc_id sys_get_current_proc_id();
int sys_proc_wait_on_proc(proc_id pid, int *retcode);
region_id sys_vm_create_anonymous_region(char *name, void **address, int addr_type,
	addr size, int wiring, int lock);
region_id sys_vm_clone_region(char *name, void **address, int addr_type,
	region_id source_region, int lock);	
// mmap file
int sys_vm_delete_region(region_id id);
region_id sys_vm_find_region_by_name(const char *name);
int sys_vm_get_region_info(region_id id, vm_region_info *info);

#endif

