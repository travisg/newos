#ifndef _ARCH_SMP_H
#define _ARCH_SMP_H

#include <stage2.h>

#define SMP_MAX_CPUS 1
	
#define arch_smp_init(x) ((int)0)
#define arch_smp_get_current_cpu() ((int)0)
#define arch_smp_send_ici(target_cpu)
#define arch_smp_send_broadcast_ici()

#endif

