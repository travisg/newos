/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_SMP_H
#define _NEWOS_KERNEL_ARCH_SMP_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

int arch_smp_init(kernel_args *ka);
int arch_smp_init_percpu(kernel_args *ka, int cpu_num);
void arch_smp_send_ici(int target_cpu);
void arch_smp_send_broadcast_ici(void);

#endif

