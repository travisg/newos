#include <string.h>
#include <printf.h>
#include <kernel.h>
#include <faults.h>
#include <debug.h>
#include <stage2.h>
#include <int.h>
#include <arch_faults.h>

int faults_init(kernel_args *ka)
{
	dprintf("init_fault_handlers: entry\n");
	return arch_faults_init(ka);
}


int general_protection_fault(int errorcode)
{
	dprintf("GENERAL PROTECTION FAULT: errcode 0x%x. Killing system.\n", errorcode);
	
//	cli();
	for(;;);
	return INT_NO_RESCHEDULE;
}

