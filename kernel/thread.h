#ifndef _THREAD_H
#define _THREAD_H

#include "stage2.h"
#include "proc.h"
#include "vm.h"

struct thread {
	struct thread *all_next;
	struct thread *next;
	char *name;
	int id;
	struct proc *proc;
	struct area *kernel_stack_area;
	struct area *user_stack_area;
	// architecture dependant section
	void *arch_info;
};

int thread_timer_interrupt();
int thread_init(struct kernel_args *ka);
int thread_kthread_exit();
#if 1
// XXX remove later
int thread_test();
#endif
#endif

