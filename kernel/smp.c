#include "smp.h"
#include "arch_smp.h"
#include "spinlock.h"

static int boot_cpu_spin[SMP_MAX_CPUS] = { 0, };

int smp_trap_non_boot_cpus(struct kernel_args *ka, int cpu)
{
	if(cpu > 0) {
		boot_cpu_spin[cpu] = 1;
		acquire_spinlock(&boot_cpu_spin[cpu]);
		return 1;
	} else {
		return 0;
	}
}

void smp_wake_up_all_non_boot_cpus()
{
	int i;
	for(i=1; i < arch_smp_get_num_cpus(); i++) {
		release_spinlock(&boot_cpu_spin[i]);
	}
}

void smp_wait_for_ap_cpus(struct kernel_args *ka)
{
	int i;
	int retry;
	do {
		retry = 0;
		for(i=1; i < ka->num_cpus; i++) {
			if(boot_cpu_spin[i] != 1)
				retry = 1;
		}
	} while(retry == 1);
}
