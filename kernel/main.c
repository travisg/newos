#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/faults.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/dev.h>
#include <devs.h>

#include <arch_cpu.h>

#include <stage2.h>

int _start(kernel_args *oldka, int cpu)
{
	kernel_args ka;
	
	memcpy(&ka, oldka, sizeof(kernel_args));

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
	
#if 0
		// XXX remove
		vfs_test();
#endif	
#if 1
		// XXX remove
		thread_test();
#endif
		kprintf("Welcome to the kernel!\n");
	
		vm_dump_areas(vm_get_kernel_aspace());
		
		smp_wake_up_all_non_boot_cpus();
		smp_enable_ici(); // ici's were previously being ignored
		thread_start_threading();
	}
	int_enable_interrupts();

#if 0
	if(1) {
		#define _PACKED __attribute__((packed)) 	
		struct gdt_idt_descr {
			unsigned short a;
			unsigned int *b;
		} _PACKED descr;		
		unsigned int foo;
		int i;

		dprintf("I'm cpu %d!\n", cpu);
						
		asm("sgdt %0;"
			: : "m" (descr));
		
		dprintf("gdt %d: len = %d, addr = 0x%x\n", cpu, descr.a, descr.b);

		asm("sidt %0;"
			: : "m" (descr));
		
		dprintf("idt %d: len = %d, addr = 0x%x\n", cpu, descr.a, descr.b);
//		for(i=0; i < 32; i++)
//			dprintf("0x%x ", descr.b[i]);		
//		dprintf("\n");

		asm("mov %%cr3,%0"
			: "=g" (foo));
		dprintf("%d pgdir at 0x%x\n", cpu, foo);
		
		asm("mov %%esp,%0"
			: "=g" (foo));
		dprintf("%d esp at 0x%x\n", cpu, foo);

		asm("mov %%cr0,%0"
			: "=g" (foo));
		dprintf("%d cr0 = 0x%x\n", cpu, foo);

		asm("mov %%cr4,%0"
			: "=g" (foo));
		dprintf("%d cr4 = 0x%x\n", cpu, foo);
	}
#endif
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
/*
	{
		static char c[] = "a";
		
		for(;;) {
			c[0]++;
			if(c[0] > 'z')
				c[0] = 'a';
			con_puts_xy(c, 79-cpu, 23);
		}
	}
*/
	for(;;);
	
	return 0;
}

