#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include <stage2.h>

// must match MAX_BOOT_CPUS in stage2.h
#define SMP_MAX_CPUS 2

int arch_smp_init(kernel_args *ka);
int arch_smp_get_num_cpus();
int arch_smp_get_current_cpu();
void arch_smp_send_ici(int target_cpu);
void arch_smp_send_broadcast_ici();
int i386_smp_interrupt(int vector);
void arch_smp_ack_interrupt();
#endif

