#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include <kernel.h>
#include <stage2.h>

// must match MAX_BOOT_CPUS in stage2.h
#define SMP_MAX_CPUS 4

int arch_smp_init(kernel_args *ka);
int arch_smp_get_current_cpu();
void arch_smp_send_ici(int target_cpu);
void arch_smp_send_broadcast_ici();
int i386_smp_interrupt(int vector);
void arch_smp_ack_interrupt();
int arch_smp_set_apic_timer(time_t relative_timeout);
int arch_smp_clear_apic_timer();

#endif

