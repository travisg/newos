#include "kernel.h"
#include "console.h"
#include "debug.h"
#include "string.h"
#include "faults.h"
#include "int.h"
#include "vm.h"
#include "timer.h"
#include "proc.h"

#include "stage2.h"

int _start(struct kernel_args *oldka)
{
	struct kernel_args ka;
	
	memcpy(&ka, oldka, sizeof(struct kernel_args));

	// setup debug output
	dbg_init(&ka);
	dbg_set_serial_debug(true);
	dprintf("Welcome to kernel debugger output!\n");

	// init modules	
	int_init(&ka);
	vm_init(&ka);
	int_init2(&ka);
	faults_init(&ka);
	con_init(&ka);
	timer_init(&ka);
	proc_init(&ka);
	thread_init(&ka);

#if 1
	// XXX remove
	thread_test();
#endif
	kprintf("Welcome to the kernel!\n");

//	int_enable_interrupts();
	
	kprintf("main: done... spinning forever\n");
	for(;;);
	
	return 0;
}

