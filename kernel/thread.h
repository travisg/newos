#ifndef _THREAD_H
#define _THREAD_H

#include <stage2.h>
#include <proc.h>
#include <vm.h>
#include <spinlock.h>

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
	THREAD_STATE_MARKED_FOR_DEATH, // ready for manny to kill it
};

struct thread {
	struct thread *all_next;
	struct thread *proc_next;
	struct thread *q_next;
	char *name;
	int id;
	int priority;
	int state;
	int next_state;
	int sem_count;
	sem_id blocked_sem_id;
	struct proc *proc;
	struct area *kernel_stack_area;
	struct area *user_stack_area;
	// architecture dependant section
	void *arch_info;
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

void thread_resched();
void thread_snooze(long long time);
int thread_init(kernel_args *ka);
void thread_kthread_exit();
struct thread *thread_get_current_thread();
thread_id thread_get_current_thread_id();
struct thread *thread_get_thread_struct(thread_id id);
struct thread *thread_get_thread_struct_locked(thread_id id);

#if 1
// XXX remove later
int thread_test();
#endif
#endif

