/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/faults.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <dev/devs.h>

#include <libc/string.h>

#include <kernel/arch/cpu.h>

int _start(kernel_args *oldka, int cpu)
{
	kernel_args ka;
	
	memcpy(&ka, oldka, sizeof(kernel_args));

	smp_set_num_cpus(ka.num_cpus);

	// if we're not a boot cpu, spin here until someone wakes us up
	if(smp_trap_non_boot_cpus(&ka, cpu) == 0) {
		// we're the boot processor, so wait for all of the APs to enter the kernel
		smp_wait_for_ap_cpus(&ka);
		
		// setup debug output
		dbg_init(&ka);
		dbg_set_serial_debug(true);
		dprintf("Welcome to kernel debugger output!\n");
	
		// init modules
		arch_cpu_init(&ka);
		int_init(&ka);

		vm_init(&ka);
		dprintf("vm up\n");
		
		// now we can use the heap and create areas
		dbg_init2(&ka);
		int_init2(&ka);
		
		faults_init(&ka);
		smp_init(&ka);
		timer_init(&ka);
		
		arch_cpu_init2(&ka);
		
		sem_init(&ka);
		dprintf("sem up\n");

		// now we can create and use semaphores
		vm_init_postsem(&ka);
		vfs_init(&ka);
		thread_init(&ka);
		dev_init(&ka);
		devs_init(&ka);
		con_init(&ka);
		kprintf("durn\n");
	
#if 1
		// XXX remove
		vfs_test();
#endif	
#if 1
		// XXX remove
		thread_test();
#endif
		kprintf("Welcome to the kernel!\n");
		
		smp_wake_up_all_non_boot_cpus();
		smp_enable_ici(); // ici's were previously being ignored
		thread_start_threading();
	}
	int_enable_interrupts();

#if 0
	if(cpu == 1) {
		dprintf("sending intercpu interrupt\n");
		smp_send_ici(0, SMP_MSG_INVL_PAGE, 0x80000000, NULL);
	}
#endif

#if 0
	panic("debugger_test\n");	
#endif
	dprintf("main: done... spinning forever on cpu %d\n", cpu);
	for(;;);
	
	return 0;
}

