#ifndef _SMP_H
#define _SMP_H

#include "stage2.h"

#include "arch_smp.h"
#define smp_trap_non_boot_cpus(ka, cpu) arch_smp_trap_non_boot_cpus(ka, cpu)
#define smp_wake_up_cpu(cpu) arch_smp_wake_up_cpu(cpu)
#define smp_init(ka) arch_smp_init(ka)
#define smp_get_num_cpus() arch_smp_get_num_cpus()
#define smp_get_current_cpu() arch_smp_get_current_cpu()

#endif

