/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_I386_THREAD_H
#define _NEWOS_KERNEL_ARCH_I386_THREAD_H

#include <kernel/arch/cpu.h>

void i386_push_iframe(struct iframe *frame);
void i386_pop_iframe(void);
struct iframe *i386_get_curr_iframe(void);

static inline struct thread *arch_thread_get_current_thread(void)
{
    struct thread *t;
    read_dr3(t);
    return t;
}

static inline void arch_thread_set_current_thread(struct thread *t)
{
    write_dr3(t);
}

#endif

