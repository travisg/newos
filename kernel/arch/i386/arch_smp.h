#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include "stage2.h"

#define arch_smp_init(ka) ((int)0)
#define arch_smp_get_num_cpus() ((int)1)
#define arch_smp_get_current_cpu() ((int)0)

#endif

