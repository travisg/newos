#ifndef _SMP_H
#define _SMP_H

#include "stage2.h"


int smp_trap_non_boot_cpus(struct kernel_args *ka, int cpu);
void smp_wake_up_all_non_boot_cpus();
void smp_wait_for_ap_cpus(struct kernel_args *ka);

#include "arch_smp.h"
#define smp_send_ici(cpu) arch_smp_send_ici(cpu)
#define smp_send_broadcast_ici() arch_smp_send_broadcast_ici()
#define smp_init(ka) arch_smp_init(ka)
#define smp_get_num_cpus() arch_smp_get_num_cpus()
#define smp_get_current_cpu() arch_smp_get_current_cpu()

#endif

