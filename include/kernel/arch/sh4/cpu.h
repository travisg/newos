/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_SH4_CPU_H
#define _NEWOS_KERNEL_ARCH_SH4_CPU_H

#include <arch/cpu.h>
#include <arch/sh4/sh4.h>
#include <arch/sh4/vcpu.h>

unsigned int get_fpscr();
unsigned int get_sr();
void sh4_context_switch(unsigned int **old_sp, unsigned int *new_sp);
void sh4_function_caller();
void sh4_set_kstack(addr_t kstack);
void sh4_enter_uspace(addr_t entry, void *args, addr_t ustack_top);
void sh4_set_user_pgdir(addr_t pgdir);
void sh4_invl_page(addr_t va);
void sh4_switch_stack_and_call(addr_t stack, void (*func)(void *), void *arg);


struct arch_cpu_info {
    // empty
};

#endif

