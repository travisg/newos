#include "arch_cpu.h"
#include "stage2.h"

int arch_cpu_init(kernel_args *ka)
{
	setup_system_time(ka->system_time_cv_factor);

	return 0;
}
