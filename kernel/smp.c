#include <string.h>

#include "console.h"
#include "debug.h"
#include "int.h"
#include "smp.h"
#include "spinlock.h"
#include "vm.h"

#include "arch_smp.h"
#include "arch_pmap.h"

struct smp_msg {
	struct smp_msg *next;
	int    message;
	unsigned int data;
	void   *data_ptr;
};

static int boot_cpu_spin[SMP_MAX_CPUS] = { 0, };

static struct smp_msg **smp_msgs = NULL;
static int msg_spinlock = 0;

int smp_intercpu_int_handler()
{
	int curr_cpu = smp_get_current_cpu();
	struct smp_msg *msg;
	int retval = INT_NO_RESCHEDULE;
	
	acquire_spinlock(&msg_spinlock);
	
	msg = smp_msgs[curr_cpu];
	smp_msgs[curr_cpu] = msg->next;

	release_spinlock(&msg_spinlock);

	switch(msg->message) {
		case SMP_MSG_INVL_PAGE:
			arch_pmap_invl_page(msg->data);
			break;
		case SMP_MSG_RESCHEDULE:
			retval = INT_RESCHEDULE;
			break;
		case SMP_MSG_1:
		default:
			kprintf("smp_intercpu_int_handler: got message %d\n", msg->message);
	}

	if(msg->data_ptr != NULL)
		kfree(msg->data_ptr);
	kfree(msg);

	return retval;
}

void smp_send_ici(int target_cpu, int message, unsigned int data, void *data_ptr)
{
	struct smp_msg *msg;
	
	msg = (struct smp_msg *)kmalloc(sizeof(struct smp_msg));

	msg->message = message;
	msg->data = data;
	msg->data_ptr = data_ptr;
		
	acquire_spinlock(&msg_spinlock);
	msg->next = smp_msgs[target_cpu];
	smp_msgs[target_cpu] = msg;
	release_spinlock(&msg_spinlock);

	arch_smp_send_ici(target_cpu);
}

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

int smp_init(struct kernel_args *ka)
{	
	smp_msgs = (struct smp_msg **)kmalloc(sizeof(struct smp_msg *) * ka->num_cpus);
	if(smp_msgs == NULL)
		return 1;
	
	memset(smp_msgs, 0, sizeof(struct smp_msg *) * ka->num_cpus);

	msg_spinlock = 0;
	
	return arch_smp_init(ka);
}
