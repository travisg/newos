#include <kernel/kernel.h>
#include <string.h>
#include <kernel/faults.h>
#include <kernel/faults_priv.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <kernel/arch/i386/interrupts.h>

#include <stage2.h>

int arch_faults_init(kernel_args *ka)
{
/* now hardcoded in arch_int.c
	set_intr_gate(8, &_double_fault_int);
	set_intr_gate(13, &_general_protection_fault_int);
*/
	return 0;
}

int i386_general_protection_fault(int errorcode)
{
	return general_protection_fault(errorcode);
}

int i386_double_fault(int errorcode)
{
	kprintf("double fault! errorcode = 0x%x\n", errorcode);
	dprintf("double fault! errorcode = 0x%x\n", errorcode);
	for(;;);
	return INT_NO_RESCHEDULE;
}

