/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _LIBSYS_SYSCALLS_H
#define _LIBSYS_SYSCALLS_H

#include <kernel/ktypes.h>
#include <newos/types.h>
#include <newos/defines.h>
#include <sys/resource.h>

typedef enum {
	STREAM_TYPE_ANY = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
} stream_type;

typedef enum {
	SEEK_SET = 0,
	SEEK_CUR,
	SEEK_END
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
	addr base;
	addr size;
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
	proc_id id;
	char name[SYS_MAX_OS_NAME_LEN];
	int state;
	int num_threads;
};

struct thread_info {
	thread_id id;
	proc_id owner_proc_id;

	char name[SYS_MAX_OS_NAME_LEN];
	int state;

	addr user_stack_base;

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

#ifdef __cplusplus
extern "C" {
#endif

int sys_null();

/* fs api */
int sys_mount(const char *path, const char *device, const char *fs_name, void *args);
int sys_unmount(const char *path);
int sys_sync();
int sys_open(const char *path, stream_type st, int omode);
int sys_close(int fd);
int sys_fsync(int fd);
ssize_t sys_read(int fd, void *buf, off_t pos, ssize_t len);
ssize_t sys_write(int fd, const void *buf, off_t pos, ssize_t len);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_create(const char *path, stream_type stream_type);
int sys_unlink(const char *path);
int sys_rename(const char *oldpath, const char *newpath);
int sys_rstat(const char *path, struct file_stat *stat);
int sys_wstat(const char *path, struct file_stat *stat, int stat_mask);
int sys_getcwd(char* buf, size_t size);
int sys_setcwd(const char* path);
int sys_dup(int fd);
int sys_dup2(int ofd, int nfd);

bigtime_t sys_system_time();
int sys_snooze(bigtime_t time);
int sys_getrlimit(int resource, struct rlimit * rlp);
int sys_setrlimit(int resource, const struct rlimit * rlp);

/* sem functions */
sem_id sys_sem_create(int count, const char *name);
int sys_sem_delete(sem_id id);
int sys_sem_acquire(sem_id id, int count);
int sys_sem_acquire_etc(sem_id id, int count, int flags, bigtime_t timeout);
int sys_sem_release(sem_id id, int count);
int sys_sem_release_etc(sem_id id, int count, int flags);
int sys_sem_get_count(sem_id id, int32* thread_count);
int sys_sem_get_sem_info(sem_id id, struct sem_info *info);
int sys_sem_get_next_sem_info(proc_id proc, uint32 *cookie, struct sem_info *info);
int sys_set_sem_owner(sem_id id, proc_id proc);

thread_id sys_get_current_thread_id();
void sys_exit(int retcode);
proc_id sys_proc_create_proc(const char *path, const char *name, char **args, int argc, int priority);
thread_id sys_thread_create_thread(const char *name, int (*func)(void *args), void *args);
int sys_thread_wait_on_thread(thread_id tid, int *retcode);
int sys_thread_suspend_thread(thread_id tid);
int sys_thread_resume_thread(thread_id tid);
int sys_thread_kill_thread(thread_id tid);
int sys_thread_get_thread_info(thread_id id, struct thread_info *info);
int sys_thread_get_next_thread_info(uint32 *cookie, proc_id pid, struct thread_info *info);
int sys_proc_kill_proc(proc_id pid);
proc_id sys_get_current_proc_id();
int sys_proc_wait_on_proc(proc_id pid, int *retcode);
int sys_proc_get_proc_info(proc_id id, struct proc_info *info);
int sys_proc_get_next_proc_info(uint32 *cookie, struct proc_info *info);
region_id sys_vm_create_anonymous_region(const char *name, void **address, int addr_type,
	addr size, int wiring, int lock);
region_id sys_vm_clone_region(const char *name, void **address, int addr_type,
	region_id source_region, int mapping, int lock);
region_id sys_vm_map_file(const char *name, void **address, int addr_type,
	addr size, int lock, int mapping, const char *path, off_t offset);
int sys_vm_delete_region(region_id id);
int sys_vm_get_region_info(region_id id, vm_region_info *info);

/* kernel port functions */
port_id		sys_port_create(int32 queue_length, const char *name);
int			sys_port_close(port_id id);
int			sys_port_delete(port_id id);
port_id		sys_port_find(const char *port_name);
int			sys_port_get_info(port_id id, struct port_info *info);
int		 	sys_port_get_next_port_info(proc_id proc, uint32 *cookie, struct port_info *info);
ssize_t		sys_port_buffer_size(port_id port);
ssize_t		sys_port_buffer_size_etc(port_id port, uint32 flags, bigtime_t timeout);
int32		sys_port_count(port_id port);
ssize_t		sys_port_read(port_id port, int32 *msg_code, void *msg_buffer, size_t buffer_size);
ssize_t		sys_port_read_etc(port_id port,	int32 *msg_code, void *msg_buffer, size_t buffer_size, uint32 flags, bigtime_t timeout);
int			sys_port_set_owner(port_id port, proc_id proc);
int			sys_port_write(port_id port, int32 msg_code, void *msg_buffer, size_t buffer_size);
int			sys_port_write_etc(port_id port, int32 msg_code, void *msg_buffer, size_t buffer_size, uint32 flags, bigtime_t timeout);

/* atomic_* ops (needed for cpus that dont support them directly) */
int sys_atomic_add(int *val, int incr);
int sys_atomic_and(int *val, int incr);
int sys_atomic_or(int *val, int incr);
int sys_atomic_set(int *val, int set_to);
int sys_test_and_set(int *val, int set_to, int test_val);

#ifdef __cplusplus
}
#endif

#endif

