#include <string.h>

#include "console.h"
#include "debug.h"
#include "int.h"
#include "smp.h"
#include "spinlock.h"
#include "vm.h"
#include "kernel.h"

#include "arch_cpu.h"
#include "arch_smp.h"
#include "arch_pmap.h"

struct smp_msg {
	struct smp_msg *next;
	int            message;
	unsigned int   data;
	void           *data_ptr;
	int            ref_count;
	unsigned int   proc_bitmap;
};

static int boot_cpu_spin[SMP_MAX_CPUS] = { 0, };

static struct smp_msg **smp_msgs = NULL;
static struct smp_msg *smp_broadcast_msgs = NULL;
static int msg_spinlock = 0;

int smp_intercpu_int_handler()
{
	int curr_cpu = smp_get_current_cpu();
	struct smp_msg *msg;
	int retval = INT_NO_RESCHEDULE;

	acquire_spinlock(&msg_spinlock);
	
	msg = smp_msgs[curr_cpu];
	if(msg != NULL) {
		smp_msgs[curr_cpu] = msg->next;
		msg->ref_count--;
	} else {
		// try getting one from the broadcast mailbox
		struct smp_msg *last_msg = NULL;

		msg = smp_broadcast_msgs;
		while(msg != NULL) {
			if(CHECK_BIT(msg->proc_bitmap, curr_cpu) != 0) {
				// we have handled this one already
				last_msg = msg;
				msg = msg->next;
				continue;
			}
			
			// mark it so we wont try to process this one again
			msg->proc_bitmap = SET_BIT(msg->proc_bitmap, curr_cpu);
			msg->ref_count--;
			if(msg->ref_count == 0) {
				// pull it out of the linked list
				if(last_msg == NULL) {
					smp_broadcast_msgs = msg->next;
				} else {
					last_msg = msg->next;
				}
			}
			break;
		}
	}
	
	release_spinlock(&msg_spinlock);

	if(msg == NULL)
		return retval;

	switch(msg->message) {
		case SMP_MSG_INVL_PAGE:
			arch_pmap_invl_page(msg->data);
			break;
		case SMP_MSG_RESCHEDULE:
			retval = INT_RESCHEDULE;
			break;
		case SMP_MSG_CPU_HALT:
			kprintf("cpu %d halted!\n", curr_cpu);
			dprintf("cpu %d halted!\n", curr_cpu);
			int_disable_interrupts();
			for(;;);
			break;
		case SMP_MSG_1:
		default:
			kprintf("smp_intercpu_int_handler: got unknown message %d\n", msg->message);
	}

	if(msg->ref_count == 0) {
		if(msg->data_ptr != NULL)
			kfree(msg->data_ptr);
		kfree(msg);
	}

	return retval;
}

void smp_send_ici(int target_cpu, int message, unsigned int data, void *data_ptr)
{
	struct smp_msg *msg;
	int state;
	
	if(arch_smp_get_num_cpus() > 1) {
		msg = (struct smp_msg *)kmalloc(sizeof(struct smp_msg));
	
		msg->message = message;
		msg->data = data;
		msg->data_ptr = data_ptr;
		msg->ref_count = 1;
			
		state = int_disable_interrupts();
		acquire_spinlock(&msg_spinlock);
		
		msg->next = smp_msgs[target_cpu];
		smp_msgs[target_cpu] = msg;
		
		arch_smp_send_ici(target_cpu);

		release_spinlock(&msg_spinlock);
		int_restore_interrupts(state);
	}
}

void smp_send_broadcast_ici(int message, unsigned int data, void *data_ptr)
{
	struct smp_msg *msg;
	int state;
	int num_cpus = arch_smp_get_num_cpus();
	
	if(num_cpus > 1) {
		msg = (struct smp_msg *)kmalloc(sizeof(struct smp_msg));
	
		msg->message = message;
		msg->data = data;
		msg->data_ptr = data_ptr;
		msg->ref_count = num_cpus - 1;
		msg->proc_bitmap = 0;
			
		state = int_disable_interrupts();
		acquire_spinlock(&msg_spinlock);
		
		msg->next = smp_broadcast_msgs;
		smp_broadcast_msgs = msg;

		arch_smp_send_broadcast_ici();

		release_spinlock(&msg_spinlock);
		int_restore_interrupts(state);
	}
}

int smp_trap_non_boot_cpus(struct kernel_args *ka, int cpu)
{
	TOUCH(ka);

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
	unsigned int i;
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
	if(ka->num_cpus > 1) {
		smp_msgs = (struct smp_msg **)kmalloc(sizeof(struct smp_msg *) * ka->num_cpus);
		if(smp_msgs == NULL)
			return 1;
		
		memset(smp_msgs, 0, sizeof(struct smp_msg *) * ka->num_cpus);
	
		msg_spinlock = 0;
	}	
	return arch_smp_init(ka);
}
