/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _THREAD_H
#define _THREAD_H

#include <boot/stage2.h>
#include <kernel/vm.h>
#include <kernel/smp.h>
#include <kernel/arch/thread_struct.h>

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

#define SIG_NONE 0
#define SIG_SUSPEND	1
#define SIG_KILL 2

struct proc {
	struct proc *next;
	proc_id id;
	char name[SYS_MAX_OS_NAME_LEN];
	int num_threads;
	int state;
	int pending_signals;
	void *ioctx;
	aspace_id aspace_id;
	vm_address_space *aspace;
	vm_address_space *kaspace;
	struct thread *main_thread;
	struct thread *thread_list;
	struct arch_proc arch_info;
};

struct thread {
	struct thread *all_next;
	struct thread *proc_next;
	struct thread *q_next;
	thread_id id;
	char name[SYS_MAX_OS_NAME_LEN];
	int priority;
	int state;
	int next_state;
	int pending_signals;
	bool in_kernel;
	sem_id sem_blocking;
	int sem_count;
	int sem_acquire_count;
	int sem_deleted_retcode;
	int sem_errcode;
	int sem_flags;
	addr fault_handler;
	addr entry;
	void *args;
	struct proc *proc;
	sem_id return_code_sem;
	region_id kernel_stack_region_id;
	addr kernel_stack_base;
	region_id user_stack_region_id;
	addr user_stack_base;
	// architecture dependant section
	struct arch_thread arch_info;
};

struct thread_queue {
	struct thread *head;
	struct thread *tail;
};

struct proc_info {
	proc_id id;
	char name[SYS_MAX_OS_NAME_LEN];
	int state;
	int num_threads;
};

void thread_enqueue(struct thread *t, struct thread_queue *q);
struct thread *thread_lookat_queue(struct thread_queue *q);
struct thread *thread_dequeue(struct thread_queue *q);
struct thread *thread_dequeue_id(struct thread_queue *q, thread_id thr_id);
struct thread *thread_lookat_run_q(int priority);
void thread_enqueue_run_q(struct thread *t);
struct thread *thread_dequeue_run_q(int priority);
void thread_atkernel_entry(void); // called when the thread enters the kernel on behalf of the thread
void thread_atkernel_exit(void);

int thread_suspend_thread(thread_id id);
int thread_resume_thread(thread_id id);
int thread_set_priority(thread_id id, int priority);
void thread_resched(void);
void thread_start_threading(void);
void thread_snooze(bigtime_t time);
int thread_init(kernel_args *ka);
void thread_exit(int retcode);
int thread_kill_thread(thread_id id);
int thread_kill_thread_nowait(thread_id id);
struct thread *thread_get_current_thread(void);
struct thread *thread_get_thread_struct(thread_id id);
thread_id thread_get_current_thread_id(void);
int thread_wait_on_thread(thread_id id, int *retcode);

thread_id thread_create_user_thread(char *name, proc_id pid, addr entry, void *args);
thread_id thread_create_kernel_thread(const char *name, int (*func)(void *args), void *args);

struct proc *proc_get_kernel_proc(void);
proc_id proc_create_proc(const char *path, const char *name, char **args, int argc, int priority);
int proc_kill_proc(proc_id);
int proc_wait_on_proc(proc_id id, int *retcode);
proc_id proc_get_kernel_proc_id(void);
proc_id proc_get_current_proc_id(void);
char **user_proc_get_arguments(void);
int user_proc_get_arg_count(void);

// used in syscalls.c
int user_thread_wait_on_thread(thread_id id, int *uretcode);
proc_id user_proc_create_proc(const char *path, const char *name, char **args, int argc, int priority);
int user_proc_wait_on_proc(proc_id id, int *uretcode);
thread_id user_thread_create_user_thread(char *uname, proc_id pid, addr entry, void *args);
int user_thread_snooze(bigtime_t time);
int user_proc_get_table(struct proc_info *pi, size_t len);

#if 1
// XXX remove later
int thread_test(void);
#endif
#endif

