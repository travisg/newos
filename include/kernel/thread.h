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
	THREAD_STATE_MARKED_FOR_DEATH, // ready for manny to kill it
};

struct proc {
	struct proc *next;
	proc_id id;
	char *name;
	void *ioctx;
	void *cwd;
	void *args;
	vm_address_space *kaspace;
	vm_address_space *aspace;
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
	int sem_count;
	sem_id blocked_sem_id;
	void *args;
	struct proc *proc;
	vm_region *kernel_stack_region;
	vm_region *user_stack_region;
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
void thread_resume_thread(struct thread *t);

void thread_resched();
void thread_start_threading();
void thread_snooze(time_t time);
int thread_init(kernel_args *ka);
void thread_kthread_exit();
struct thread *thread_get_current_thread();
thread_id thread_get_current_thread_id();
struct thread *thread_get_thread_struct(thread_id id);
struct thread *thread_get_thread_struct_locked(thread_id id);

struct thread *thread_create_user_thread(char *name, struct proc *p, int priority, addr entry);
struct thread *thread_create_kernel_thread(const char *name, int (*func)(void), int priority);

struct proc *proc_get_kernel_proc();
struct proc *proc_create_proc(const char *path, const char *name, int priority);

#if 1
// XXX remove later
int thread_test();
#endif
#endif

