/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_THREAD_H
#define _KERNEL_THREAD_H

#include <boot/stage2.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <kernel/arch/thread_struct.h>
#include <sys/resource.h>
#include <signal.h>
#include <kernel/list.h>

#define THREAD_IDLE_PRIORITY 0
#define THREAD_NUM_PRIORITY_LEVELS 64
#define THREAD_NUM_RT_PRIORITY_LEVELS 16
#define THREAD_MIN_PRIORITY (THREAD_IDLE_PRIORITY + 1)
#define THREAD_MAX_PRIORITY (THREAD_NUM_PRIORITY_LEVELS - THREAD_NUM_RT_PRIORITY_LEVELS - 1)
#define THREAD_MIN_RT_PRIORITY (THREAD_MAX_PRIORITY + 1)
#define THREAD_MAX_RT_PRIORITY (THREAD_NUM_PRIORITY_LEVELS - 1)

#define THREAD_LOWEST_PRIORITY    THREAD_MIN_PRIORITY
#define THREAD_LOW_PRIORITY       12
#define THREAD_MEDIUM_PRIORITY    24
#define THREAD_HIGH_PRIORITY      36
#define THREAD_HIGHEST_PRIORITY   THREAD_MAX_PRIORITY

#define THREAD_RT_LOW_PRIORITY    THREAD_MIN_RT_PRIORITY
#define THREAD_RT_HIGH_PRIORITY   THREAD_MAX_RT_PRIORITY

extern spinlock_t thread_spinlock;
#define GRAB_THREAD_LOCK() acquire_spinlock(&thread_spinlock)
#define RELEASE_THREAD_LOCK() release_spinlock(&thread_spinlock)

enum {
	THREAD_STATE_READY = 0,   // ready to run
	THREAD_STATE_RUNNING, // running right now somewhere
	THREAD_STATE_WAITING, // blocked on something
	THREAD_STATE_SUSPENDED, // suspended, not in queue
	THREAD_STATE_FREE_ON_RESCHED, // free the thread structure upon reschedule
	THREAD_STATE_BIRTH	// thread is being created
};

enum {
	PROC_STATE_NORMAL,	// normal state
	PROC_STATE_BIRTH,	// being contructed
	PROC_STATE_DEATH	// being killed
};

enum {
	KERNEL_TIME,
	USER_TIME
};

// args to proc_create_proc()
enum {
	PROC_FLAG_SUSPENDED = 1,
	PROC_FLAG_NEW_PGROUP = 2,
	PROC_FLAG_NEW_SESSION = 4
};

struct proc {
	struct proc *next;
	struct proc *parent;
	struct list_node siblings_node;	
	struct list_node children;

	// process id, process group id, session id
	proc_id id;
	pgrp_id pgid;
	sess_id sid;

	// list of other threads in the same process group and session
	struct list_node pg_node;
	struct list_node session_node;

	char name[SYS_MAX_OS_NAME_LEN];
	int num_threads;
	int state;
	void *ioctx;
	aspace_id aspace_id;
	struct vm_address_space *aspace;
	struct vm_address_space *kaspace;
	struct thread *main_thread;
	struct list_node thread_list;
	struct arch_proc arch_info;
};

// tracks the state of the fpu state for each thread
enum {
	FPU_STATE_UNINITIALIZED = 0,	// initial state
	FPU_STATE_ACTIVE,				// active and on the current cpu
	FPU_STATE_SAVED,				// saved in the thread structure
	FPU_STATE_UNSAVED				// resident in one of the cpus, not active
};

struct thread {
	struct thread *next;
	thread_id id;
	struct list_node proc_node;
	struct list_node q_node;
	struct proc *proc;
	char name[SYS_MAX_OS_NAME_LEN];
	int priority;
	int state;
	int next_state;
	union cpu_ent *cpu;
	bool in_kernel;

	union cpu_ent *fpu_cpu; // this cpu holds our fpu state
	int fpu_state;

	int int_disable_level;

	sem_id sem_blocking;
	int sem_count;
	int sem_acquire_count;
	int sem_deleted_retcode;
	int sem_errcode;
	int sem_flags;

	addr_t fault_handler;
	addr_t entry;
	void *args;
	sem_id return_code_sem;
	region_id kernel_stack_region_id;
	addr_t kernel_stack_base;
	region_id user_stack_region_id;
	addr_t user_stack_base;

	bigtime_t user_time;
	bigtime_t kernel_time;
	bigtime_t last_time;
	int last_time_type; // KERNEL_TIME or USER_TIME

	sigset_t		sig_pending;
	sigset_t		sig_block_mask;
	struct sigaction sig_action[32];
	struct timer_event alarm_event;

	// architecture dependant section
	struct arch_thread arch_info;
};

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

#include <kernel/arch/thread.h>

void thread_enqueue(struct thread *t, struct list_node *q);
struct thread *thread_lookat_queue(struct list_node *q);
struct thread *thread_dequeue(struct list_node *q);
void thread_dequeue_thread(struct thread *t);
struct thread *thread_lookat_run_q(int priority);
void thread_enqueue_run_q(struct thread *t);
void thread_atkernel_entry(void); // called when the thread enters the kernel on behalf of the thread
void thread_atkernel_exit(void); // called when the kernel exits the kernel on behalf of the thread
int thread_atinterrupt_exit(void); // called upon exit of an interrupt. Attempts signal delivery

int thread_suspend_thread(thread_id id);
int thread_resume_thread(thread_id id);
int thread_set_priority(thread_id id, int priority);
void thread_resched(void);
void thread_start_threading(void);
int thread_snooze(bigtime_t time);
void thread_yield(void);
int thread_init(kernel_args *ka);
int thread_init_percpu(int cpu_num);
void thread_exit(int retcode);
int thread_kill_thread(thread_id id);
int thread_kill_thread_nowait(thread_id id);
struct thread *thread_get_current_thread(void);
extern inline struct thread *thread_get_current_thread(void) {
	return arch_thread_get_current_thread();
}
struct thread *thread_get_thread_struct(thread_id id);
struct thread *thread_get_thread_struct_locked(thread_id id);
thread_id thread_get_current_thread_id(void);
extern inline thread_id thread_get_current_thread_id(void) {
	struct thread *t = thread_get_current_thread(); return t ? t->id : 0;
}
int thread_wait_on_thread(thread_id id, int *retcode);

thread_id thread_create_user_thread(char *name, proc_id pid, addr_t entry, void *args);
thread_id thread_create_kernel_thread(const char *name, int (*func)(void *args), void *args);

int thread_get_thread_info(thread_id id, struct thread_info *info);
int user_thread_get_thread_info(thread_id id, struct thread_info *info);
int thread_get_next_thread_info(uint32 *cookie, proc_id pid, struct thread_info *info);
int user_thread_get_next_thread_info(uint32 *cookie, proc_id pid, struct thread_info *info);

struct proc *proc_get_kernel_proc(void);
proc_id proc_create_proc(const char *path, const char *name, char **args, int argc, int priority, int flags);
int proc_kill_proc(proc_id);
int proc_wait_on_proc(proc_id id, int *retcode);
thread_id proc_get_main_thread(proc_id id);
proc_id proc_get_kernel_proc_id(void);
proc_id proc_get_current_proc_id(void);
struct proc *proc_get_current_proc(void);
char **user_proc_get_arguments(void);
int user_proc_get_arg_count(void);

int proc_get_proc_info(proc_id id, struct proc_info *info);
int user_proc_get_proc_info(proc_id id, struct proc_info *info);
int proc_get_next_proc_info(uint32 *cookie, struct proc_info *info);
int user_proc_get_next_proc_info(uint32 *cookie, struct proc_info *info);

// used in syscalls.c
int user_thread_wait_on_thread(thread_id id, int *uretcode);
proc_id user_proc_create_proc(const char *path, const char *name, char **args, int argc, int priority, int flags);
int user_proc_wait_on_proc(proc_id id, int *uretcode);
thread_id user_thread_create_user_thread(char *uname, addr_t entry, void *args);
int user_thread_set_priority(thread_id id, int priority);
int user_thread_snooze(bigtime_t time);
int user_thread_yield(void);
int user_getrlimit(int resource, struct rlimit * rlp);
int user_setrlimit(int resource, const struct rlimit * rlp);

// process group/session group stuff
int setpgid(proc_id pid, pgrp_id pgid);
pgrp_id getpgid(proc_id pid);
sess_id setsid(void);

#endif

