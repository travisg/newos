#ifndef _SMP_H
#define _SMP_H

#include <boot/stage2.h>

// intercpu messages
enum {
	SMP_MSG_INVL_PAGE = 0,
	SMP_MSG_RESCHEDULE,
	SMP_MSG_CPU_HALT,
	SMP_MSG_1,
};

enum {
	SMP_MSG_FLAG_ASYNC = 0,
	SMP_MSG_FLAG_SYNC,
};

int smp_init(kernel_args *ka);
int smp_trap_non_boot_cpus(kernel_args *ka, int cpu);
void smp_wake_up_all_non_boot_cpus();
void smp_wait_for_ap_cpus(kernel_args *ka);
void smp_send_ici(int target_cpu, int message, unsigned int data, void *data_ptr, int flags);
void smp_send_broadcast_ici(int message, unsigned int data, void *data_ptr, int flags);
int smp_enable_ici();
int smp_disable_ici();

int smp_get_num_cpus();
void smp_set_num_cpus(int num_cpus);

#include <kernel/arch/smp.h>
#define smp_get_current_cpu() arch_smp_get_current_cpu()

// spinlock functions
void acquire_spinlock(int *lock);
void release_spinlock(int *lock);

#endif

