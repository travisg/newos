/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_CPU_H
#define _ARCH_CPU_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

#ifdef ARCH_i386
#include <kernel/arch/i386/cpu.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/cpu.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/cpu.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/cpu.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/cpu.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/cpu.h>
#endif

#define PAGE_ALIGN(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

int atomic_add(int *val, int incr);
int atomic_and(int *val, int incr);
int atomic_or(int *val, int incr);
int test_and_set(int *val, int set_to);

time_t system_time();
int arch_cpu_init(kernel_args *ka);
int arch_cpu_init2(kernel_args *ka);
void reboot();

void arch_cpu_invalidate_TLB_range(addr start, addr end);
void arch_cpu_invalidate_TLB_list(addr pages[], int num_pages);
void arch_cpu_global_TLB_invalidate();

#endif

