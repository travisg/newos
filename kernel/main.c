/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>
#include <sys/errors.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/faults.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/fs/rootfs.h>
#include <kernel/fs/bootfs.h>
#include <kernel/fs/devfs.h>
#include <kernel/dev.h>
#include <kernel/net/net.h>
#include <kernel/cbuf.h>
#include <kernel/elf.h>
#include <dev/devs.h>
#include <bus/bus.h>

#include <libc/string.h>

#include <kernel/arch/cpu.h>

static kernel_args ka;

static int main2();

int _start(kernel_args *oldka, int cpu)
{
	memcpy(&ka, oldka, sizeof(kernel_args));

	smp_set_num_cpus(ka.num_cpus);

	// if we're not a boot cpu, spin here until someone wakes us up
	if(smp_trap_non_boot_cpus(&ka, cpu) == NO_ERROR) {
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

		// now we can create and use semaphores
		vm_init_postsem(&ka);
		cbuf_init();
		vfs_init(&ka);
		thread_init(&ka);
		vm_init_postthread(&ka);
		elf_init(&ka);

		// start a thread to finish initializing the rest of the system
		{
			thread_id tid;
			tid = thread_create_kernel_thread("main2", &main2, 20);
			thread_resume_thread(tid);
		}

		smp_wake_up_all_non_boot_cpus();
		smp_enable_ici(); // ici's were previously being ignored
		thread_start_threading();
	}
	int_enable_interrupts();

	dprintf("main: done... begin idle loop on cpu %d\n", cpu);
	for(;;);

	return 0;
}

static int main2()
{
	int err;

	dprintf("start of main2: initializing devices\n");

	// bootstrap the root filesystem
	bootstrap_rootfs();

	err = sys_mount("/", "rootfs");
	if(err < 0)
		panic("error mounting rootfs!\n");

	sys_setcwd("/");

	// bootstrap the bootfs
	bootstrap_bootfs();

	sys_create("/boot", STREAM_TYPE_DIR);
	err = sys_mount("/boot", "bootfs");
	if(err < 0)
		panic("error mounting bootfs\n");

	// bootstrap the devfs
	bootstrap_devfs();

	sys_create("/dev", STREAM_TYPE_DIR);
	err = sys_mount("/dev", "devfs");
	if(err < 0)
		panic("error mounting devfs\n");

	dev_init(&ka);
	bus_init(&ka);
	devs_init(&ka);
	con_init(&ka);
	net_init(&ka);

#if 0
		// XXX remove
		vfs_test();
#endif
#if 0
		// XXX remove
		thread_test();
#endif
#if 0
		vm_test();
#endif
#if 0
	panic("debugger_test\n");
#endif

	vfs_load_fs_module("/boot/iso9660fs");

	// start the init process
	{
		proc_id pid;
		pid = proc_create_proc("/boot/init", "init", 5);
		if(pid < 0)
			kprintf("error starting 'init'\n");
	}

	return 0;
}

