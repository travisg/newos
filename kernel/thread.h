#ifndef _THREAD_H
#define _THREAD_H

#include "stage2.h"
#include "proc.h"
#include "vm.h"

#define THREAD_IDLE_PRIORITY 0
#define THREAD_NUM_PRIORITY_LEVELS 64
#define THREAD_MAX_PRIORITY (THREAD_NUM_PRIORITY_LEVELS - 1)

enum {
	THREAD_STATE_RUN = 0,
	THREAD_STATE_BLOCKED
};

struct thread {
	struct thread *all_next;
	struct thread *proc_next;
	struct thread *q_next;
	char *name;
	int id;
	int priority;
	int state;
	struct proc *proc;
	struct area *kernel_stack_area;
	struct area *user_stack_area;
	// architecture dependant section
	void *arch_info;
};

int thread_resched();
int thread_init(struct kernel_args *ka);
int thread_kthread_exit();

#if 1
// XXX remove later
int thread_test();
#endif
#endif

