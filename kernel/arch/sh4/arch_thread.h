#ifndef _ARCH_THREAD_H
#define _ARCH_THREAD_H

#include "thread.h"

// architecture specific thread info
struct arch_thread {
	int foo;
};

/*
struct arch_thread *arch_thread_create_thread_struct();
void arch_thread_context_switch(struct thread *t_from, struct thread *t_to);
int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void *param));
*/

#define arch_thread_create_thread_struct() ((struct arch_thread *)NULL)
#define arch_thread_context_switch(tf, tto)
#define arch_thread_initialize_kthread_stack(t, f) ((int)0)


#endif

