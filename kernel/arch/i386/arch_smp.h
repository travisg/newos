#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include "stage2.h"

int arch_smp_init(struct kernel_args *ka);
int arch_smp_get_num_cpus();
int arch_smp_get_current_cpu();

#endif

