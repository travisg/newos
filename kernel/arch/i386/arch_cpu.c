#include <kernel/arch/cpu.h>
#include <boot/stage2.h>

int arch_cpu_init(kernel_args *ka)
{
	setup_system_time(ka->arch_args.system_time_cv_factor);

	return 0;
}
