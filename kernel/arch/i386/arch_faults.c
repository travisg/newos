#include "string.h"
#include "faults.h"
#include "vm.h"
#include "debug.h"
#include "console.h"

#include "arch_cpu.h"
#include "arch_interrupts.h"
#include "arch_int.h"

#include "stage2.h"

int arch_faults_init(struct kernel_args *ka)
{
	set_intr_gate(8, &_double_fault_int);
	set_intr_gate(13, &_general_protection_fault_int);

	return 0;
}

void i386_general_protection_fault(int errorcode)
{
	general_protection_fault(errorcode);
}

void i386_double_fault(int errorcode)
{
	kprintf("double fault! errorcode = 0x%x\n", errorcode);
	dprintf("double fault! errorcode = 0x%x\n", errorcode);
	for(;;);
}

