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
#define THREAD_MAX_PRIORITY (THREAD_NUM_PRIORITY_LEVELS - 1)

extern int thread_spinlock;
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
	int num_threads;
	int state;
	int pending_signals;
	char *name;
	void *ioctx;
	void *args;
	sem_id proc_creation_sem;
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
	char *name;
	thread_id id;
	int priority;
	int state;
	int next_state;
	int pending_signals;
	bool in_kernel;
	int sem_count;
	int sem_deleted_retcode;
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

void thread_enqueue(struct thread *t, struct thread_queue *q);
struct thread *thread_lookat_queue(struct thread_queue *q);
struct thread *thread_dequeue(struct thread_queue *q);
struct thread *thread_dequeue_id(struct thread_queue *q, thread_id thr_id);
struct thread *thread_lookat_run_q(int priority);
void thread_enqueue_run_q(struct thread *t);
struct thread *thread_dequeue_run_q(int priority);
void thread_atkernel_entry(); // called when the thread enters the kernel on behalf of the thread
void thread_atkernel_exit();

int thread_suspend_thread(thread_id id);
int thread_resume_thread(thread_id id);
void thread_resched();
void thread_start_threading();
void thread_snooze(time_t time);
int thread_init(kernel_args *ka);
void thread_exit(int retcode);
int thread_kill_thread(thread_id id);
int thread_kill_thread_nowait(thread_id id);
struct thread *thread_get_current_thread();
struct thread *thread_get_thread_struct(thread_id id);
thread_id thread_get_current_thread_id();
int thread_wait_on_thread(thread_id id, int *retcode);

thread_id thread_create_user_thread(char *name, proc_id pid, int priority, addr entry);
thread_id thread_create_kernel_thread(const char *name, int (*func)(void), int priority);

struct proc *proc_get_kernel_proc();
proc_id proc_create_proc(const char *path, const char *name, int priority);
int proc_kill_proc(proc_id);
int proc_wait_on_proc(proc_id id, int *retcode);
proc_id proc_get_current_proc_id();

#if 1
// XXX remove later
int thread_test();
#endif
#endif

