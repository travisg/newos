/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_X86_64_THREAD_H
#define _NEWOS_KERNEL_ARCH_X86_64_THREAD_H

#include <kernel/arch/cpu.h>

void x86_64_push_iframe(struct iframe *frame);
void x86_64_pop_iframe(void);
struct iframe *x86_64_get_curr_iframe(void);

static inline struct thread *arch_thread_get_current_thread(void)
{
    return (struct thread *)read_dr3();
}

static inline void arch_thread_set_current_thread(struct thread *t)
{
    write_dr3((addr_t)t);
}

#endif

