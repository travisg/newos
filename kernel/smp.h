#ifndef _SMP_H
#define _SMP_H

#include <stage2.h>

// intercpu messages
enum {
	SMP_MSG_INVL_PAGE = 0,
	SMP_MSG_RESCHEDULE,
	SMP_MSG_CPU_HALT,
	SMP_MSG_1,
};

int smp_init(kernel_args *ka);
int smp_trap_non_boot_cpus(kernel_args *ka, int cpu);
void smp_wake_up_all_non_boot_cpus();
void smp_wait_for_ap_cpus(kernel_args *ka);
void smp_send_ici(int target_cpu, int message, unsigned int data, void *data_ptr);
void smp_send_broadcast_ici(int message, unsigned int data, void *data_ptr);

int smp_intercpu_int_handler();

#include <arch_smp.h>
#define smp_get_num_cpus() arch_smp_get_num_cpus()
#define smp_get_current_cpu() arch_smp_get_current_cpu()

#endif

