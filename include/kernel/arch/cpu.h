/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_CPU_H
#define _NEWOS_KERNEL_ARCH_CPU_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <arch/cpu.h>

#define PAGE_ALIGN(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

int atomic_add(volatile int *val, int incr);
int atomic_and(volatile int *val, int incr);
int atomic_or(volatile int *val, int incr);
int atomic_set(volatile int *val, int set_to);
int test_and_set(int *val, int set_to, int test_val);

bigtime_t system_time(void);
int arch_cpu_preboot_init(kernel_args *ka);
int arch_cpu_init(kernel_args *ka);
int arch_cpu_init2(kernel_args *ka);
void reboot(void);

void arch_cpu_invalidate_TLB_range(addr_t start, addr_t end);
void arch_cpu_invalidate_TLB_list(addr_t pages[], int num_pages);
void arch_cpu_global_TLB_invalidate(void);

void arch_cpu_sync_icache(void *address, size_t len);

int arch_cpu_user_memcpy(void *to, const void *from, size_t size, addr_t *fault_handler);
int arch_cpu_user_strcpy(char *to, const char *from, addr_t *fault_handler);
int arch_cpu_user_strncpy(char *to, const char *from, size_t size, addr_t *fault_handler);
int arch_cpu_user_memset(void *s, char c, size_t count, addr_t *fault_handler);

void arch_cpu_idle(void);

#include INC_ARCH(kernel/arch,cpu.h)

#endif

