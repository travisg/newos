#include <string.h>
#include <printf.h>
#include "kernel.h"
#include "faults.h"
#include "debug.h"
#include "stage2.h"
#include "arch_faults.h"

int faults_init(struct kernel_args *ka)
{
	dprintf("init_fault_handlers: entry\n");
	return arch_faults_init(ka);
}


void general_protection_fault(int errorcode)
{
	dprintf("GENERAL PROTECTION FAULT: errcode 0x%x. Killing system.\n", errorcode);
	
//	cli();
	for(;;);
}

