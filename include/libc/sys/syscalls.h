/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _LIBSYS_SYSCALLS_H
#define _LIBSYS_SYSCALLS_H

#include <kernel/ktypes.h>
#include <newos/types.h>
#include <newos/defines.h>
#include <sys/resource.h>
#include <signal.h>

typedef enum {
	STREAM_TYPE_ANY = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
	STREAM_TYPE_PIPE
} stream_type;

typedef enum {
	_SEEK_SET = 0,
	_SEEK_CUR,
	_SEEK_END
} seek_type;

struct file_stat {
	vnode_id 	vnid;
	stream_type	type;
	off_t		size;
};

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2
#define SEM_FLAG_INTERRUPTABLE 4

#define PORT_FLAG_TIMEOUT 2

// info about a region that external entities may want to know
typedef struct vm_region_info {
	region_id id;
	addr_t base;
	addr_t size;
	int lock;
	int wiring;
	char name[SYS_MAX_OS_NAME_LEN];
} vm_region_info;

typedef struct sem_info {
	sem_id		sem;
	proc_id		proc;
	char		name[SYS_MAX_OS_NAME_LEN];
	int32		count;
	thread_id	latest_holder;
} sem_info;

typedef struct port_info {
	port_id id;
	proc_id owner;
	char name[SYS_MAX_OS_NAME_LEN];
	int32 capacity;
	int32 queue_count;
	int32 total_count;
} port_info;

struct proc_info {
	proc_id pid;
	proc_id ppid;
	pgrp_id pgid;
	sess_id sid;
	int state;
	int num_threads;
	char name[SYS_MAX_OS_NAME_LEN];
};

struct thread_info {
	thread_id id;
	proc_id owner_proc_id;

	char name[SYS_MAX_OS_NAME_LEN];
	int state;
	int priority;

	addr_t user_stack_base;

	bigtime_t user_time;
	bigtime_t kernel_time;
};

// args for the create_area funcs
enum {
	REGION_ADDR_ANY_ADDRESS = 0,
	REGION_ADDR_EXACT_ADDRESS
};

enum {
	REGION_WIRING_LAZY = 0,
	REGION_WIRING_WIRED,
	REGION_WIRING_WIRED_ALREADY,
	REGION_WIRING_WIRED_CONTIG
 };

enum {
	REGION_NO_PRIVATE_MAP = 0,
	REGION_PRIVATE_MAP
};

#define LOCK_RO        0x0
#define LOCK_RW        0x1
#define LOCK_KERNEL    0x2
#define LOCK_MASK      0x3

// state of the vm
typedef struct {
	// info about the size of memory in the system
	int physical_page_size;
	int physical_pages;

	// amount of virtual memory left to commit
	int max_commit;

	// info about the page queues
	int active_pages;
	int inactive_pages;
	int busy_pages;
	int unused_pages;
	int modified_pages;
	int modified_temporary_pages;
	int free_pages;
	int clear_pages;
	int wired_pages;

	// info about vm activity
	int page_faults;
} vm_info_t;

typedef enum {
	TIMER_MODE_ONESHOT = 0,
	TIMER_MODE_PERIODIC
} timer_mode;

#ifdef __cplusplus
extern "C" {
#endif

int _kern_null(void);

/* fs api */
int _kern_mount(const char *path, const char *device, const char *fs_name, void *args);
int _kern_unmount(const char *path);
int _kern_sync(void);
int _kern_opendir(const char *path);
int _kern_closedir(int fd);
int _kern_rewinddir(int fd);
int _kern_readdir(int fd, void *buf, size_t len);
int _kern_open(const char *path, int omode);
int _kern_close(int fd);
int _kern_fsync(int fd);
ssize_t _kern_read(int fd, void *buf, off_t pos, ssize_t len);
ssize_t _kern_write(int fd, const void *buf, off_t pos, ssize_t len);
int _kern_seek(int fd, off_t pos, seek_type seek_type);
int _kern_ioctl(int fd, int op, void *buf, size_t len);
int _kern_create(const char *path);
int _kern_unlink(const char *path);
int _kern_rename(const char *oldpath, const char *newpath);
int _kern_mkdir(const char *path);
int _kern_rmdir(const char *path);
int _kern_rstat(const char *path, struct file_stat *stat);
int _kern_wstat(const char *path, struct file_stat *stat, int stat_mask);
int _kern_getcwd(char* buf, size_t size);
int _kern_setcwd(const char* path);
int _kern_dup(int fd);
int _kern_dup2(int ofd, int nfd);

bigtime_t _kern_system_time(void);
int _kern_snooze(bigtime_t time);
int _kern_getrlimit(int resource, struct rlimit * rlp);
int _kern_setrlimit(int resource, const struct rlimit * rlp);

/* sem functions */
sem_id _kern_sem_create(int count, const char *name);
int _kern_sem_delete(sem_id id);
int _kern_sem_acquire(sem_id id, int count);
int _kern_sem_acquire_etc(sem_id id, int count, int flags, bigtime_t timeout);
int _kern_sem_release(sem_id id, int count);
int _kern_sem_release_etc(sem_id id, int count, int flags);
int _kern_sem_get_count(sem_id id, int32* thread_count);
int _kern_sem_get_sem_info(sem_id id, struct sem_info *info);
int _kern_sem_get_next_sem_info(proc_id proc, uint32 *cookie, struct sem_info *info);
int _kern_set_sem_owner(sem_id id, proc_id proc);

thread_id _kern_get_current_thread_id(void);
void _kern_exit(int retcode);
proc_id _kern_proc_create_proc(const char *path, const char *name, char **args, int argc, int priority);
thread_id _kern_thread_create_thread(const char *name, int (*func)(void *args), void *args);
int _kern_thread_set_priority(thread_id tid, int priority);
int _kern_thread_wait_on_thread(thread_id tid, int *retcode);
int _kern_thread_suspend_thread(thread_id tid);
int _kern_thread_resume_thread(thread_id tid);
int _kern_thread_kill_thread(thread_id tid);
int _kern_thread_get_thread_info(thread_id id, struct thread_info *info);
int _kern_thread_get_next_thread_info(uint32 *cookie, proc_id pid, struct thread_info *info);
int _kern_proc_kill_proc(proc_id pid);
proc_id _kern_get_current_proc_id(void);
int _kern_proc_wait_on_proc(proc_id pid, int *retcode);
int _kern_proc_get_proc_info(proc_id id, struct proc_info *info);
int _kern_proc_get_next_proc_info(uint32 *cookie, struct proc_info *info);
region_id _kern_vm_create_anonymous_region(const char *name, void **address, int addr_type,
	addr_t size, int wiring, int lock);
region_id _kern_vm_clone_region(const char *name, void **address, int addr_type,
	region_id source_region, int mapping, int lock);
region_id _kern_vm_map_file(const char *name, void **address, int addr_type,
	addr_t size, int lock, int mapping, const char *path, off_t offset);
int _kern_vm_delete_region(region_id id);
int _kern_vm_get_region_info(region_id id, vm_region_info *info);
int _kern_vm_get_vm_info(vm_info_t *uinfo);

/* kernel port functions */
port_id		_kern_port_create(int32 queue_length, const char *name);
int			_kern_port_close(port_id id);
int			_kern_port_delete(port_id id);
port_id		_kern_port_find(const char *port_name);
int			_kern_port_get_info(port_id id, struct port_info *info);
int		 	_kern_port_get_next_port_info(proc_id proc, uint32 *cookie, struct port_info *info);
ssize_t		_kern_port_buffer_size(port_id port);
ssize_t		_kern_port_buffer_size_etc(port_id port, uint32 flags, bigtime_t timeout);
int32		_kern_port_count(port_id port);
ssize_t		_kern_port_read(port_id port, int32 *msg_code, void *msg_buffer, size_t buffer_size);
ssize_t		_kern_port_read_etc(port_id port,	int32 *msg_code, void *msg_buffer, size_t buffer_size, uint32 flags, bigtime_t timeout);
int			_kern_port_set_owner(port_id port, proc_id proc);
int			_kern_port_write(port_id port, int32 msg_code, void *msg_buffer, size_t buffer_size);
int			_kern_port_write_etc(port_id port, int32 msg_code, void *msg_buffer, size_t buffer_size, uint32 flags, bigtime_t timeout);

/* atomic_* ops (needed for cpus that dont support them directly) */
int _kern_atomic_add(int *val, int incr);
int _kern_atomic_and(int *val, int incr);
int _kern_atomic_or(int *val, int incr);
int _kern_atomic_set(int *val, int set_to);
int _kern_test_and_set(int *val, int set_to, int test_val);

/* signals */
int _kern_sigaction(int sig, const struct sigaction *action, struct sigaction *old_action);
int _kern_send_signal(thread_id tid, uint signal);
int _kern_send_proc_signal(proc_id pid, uint signal);
bigtime_t _kern_set_alarm(bigtime_t time, timer_mode mode);

#ifdef __cplusplus
}
#endif

#endif

