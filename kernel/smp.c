#include "smp.h"
#include "arch_smp.h"

#if !_SMP_ARCH_INLINE_CALLS
int smp_init(struct kernel_args *ka)
{
	return arch_smp_init(ka, &num_cpus);
}

int smp_get_num_cpus()
{
	return arch_smp_get_num_cpus;
}

int smp_get_current_cpu()
{
	return arch_smp_get_current_cpu();
}
#endif

