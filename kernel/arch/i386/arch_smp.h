#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include "stage2.h"

// must match MAX_BOOT_CPUS in stage2.h
#define SMP_MAX_CPUS 2

int arch_smp_trap_non_boot_cpus(struct kernel_args *ka, int cpu);
void arch_smp_wake_up_cpu(int cpu);
int arch_smp_init(struct kernel_args *ka);
int arch_smp_get_num_cpus();
int arch_smp_get_current_cpu();
int i386_smp_interrupt(int vector);

#endif

