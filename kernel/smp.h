#ifndef _SMP_H
#define _SMP_H

#include "stage2.h"
#include "arch_smp.h"

#define _SMP_ARCH_INLINE_CALLS 1
#if _SMP_ARCH_INLINECALLS
#define smp_init(ka) arch_smp_init(ka)
#define smp_get_num_cpus() arch_smp_get_num_cpus
#define smp_get_current_cpu() arch_smp_get_num_cpus
#else
int smp_init(struct kernel_args *ka);
int smp_get_num_cpus();
int smp_get_current_cpu();
#endif

#endif

