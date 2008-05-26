/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cpu.h>
#include <kernel/arch/cpu.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <kernel/debug.h>
#include <kernel/arch/x86_64/selector.h>
#include <kernel/arch/int.h>
#include <kernel/arch/x86_64/interrupts.h>
#include <newos/errors.h>

#include <boot/stage2.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* a few debug functions that get added to the kernel debugger menu */
static void dbg_in(int argc, char **argv);
static void dbg_out(int argc, char **argv);

static struct tss_64 **tss;
static int *tss_loaded;

static desc_table *gdt = 0;

int arch_cpu_preboot_init(kernel_args *ka)
{
	write_dr3(0);
	return 0;
}

int arch_cpu_init(kernel_args *ka)
{
	setup_system_time(ka->arch_args.system_time_cv_factor);

	return 0;
}

int arch_cpu_init_percpu(kernel_args *ka, int curr_cpu)
{
//	detect_cpu(ka, curr_cpu);

	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	region_id rid;
	struct descriptor_64 *tss_d;
	unsigned int i;

	// account for the segment descriptors
	gdt = (desc_table *)ka->arch_args.vir_gdt;
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "gdt", (void **)&gdt,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	tss = kmalloc(sizeof(struct tss_64 *) * ka->num_cpus);
	if(tss == NULL) {
		panic("arch_cpu_init2: could not allocate buffer for tss pointers\n");
		return ERR_NO_MEMORY;
	}

	tss_loaded = kmalloc(sizeof(int) * ka->num_cpus);
	if(tss == NULL) {
		panic("arch_cpu_init2: could not allocate buffer for tss booleans\n");
		return ERR_NO_MEMORY;
	}
	memset(tss_loaded, 0, sizeof(int) * ka->num_cpus);

	for(i=0; i<ka->num_cpus; i++) {
		char tss_name[16];

		sprintf(tss_name, "tss%d", i);
		rid = vm_create_anonymous_region(vm_get_kernel_aspace_id(), tss_name, (void **)&tss[i],
			REGION_ADDR_ANY_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
		if(rid < 0) {
			panic("arch_cpu_init2: unable to create region for tss\n");
			return ERR_NO_MEMORY;
		}

		memset(tss[i], 0, sizeof(struct tss_64));

		// add TSS descriptor for this new TSS
		tss_d = (struct descriptor_64 *)&gdt[5 + i];
		tss_d->limit_00_15 = sizeof(struct tss_64) & 0xffff;
		tss_d->limit_19_16 = 0; // it's not this long
		tss_d->base_00_15 = (addr_t)tss[i] & 0xffff;
		tss_d->base_23_16 = ((addr_t)tss[i] >> 16) & 0xff;
		tss_d->base_31_24 = ((addr_t)tss[i] >> 24) & 0xff;
		tss_d->base_63_32 = ((addr_t)tss[i] >> 32) & 0xffffffff;
		tss_d->type = 0x9;
		tss_d->system = 0;
		tss_d->dpl = 0;
		tss_d->present = 1;
		tss_d->zero = 0;
		tss_d->granularity = 1;
		tss_d->reserved = 0;
		tss_d->zero2 = 0;
		tss_d->reserved2 = 0;
	}

	// set up a few debug commands (in, out)
	dbg_add_command(&dbg_in, "in", "read I/O port");
	dbg_add_command(&dbg_out, "out", "write I/O port");

	return 0;
}

desc_table *x86_64_get_gdt(void)
{
	return gdt;
}

void x86_64_set_kstack(addr_t kstack)
{
	int curr_cpu = smp_get_current_cpu();

//	dprintf("x86_64_set_kstack: kstack 0x%x, cpu %d\n", kstack, curr_cpu);
	if(tss_loaded[curr_cpu] == 0) {
		short seg = (TSS + 8*curr_cpu);
		asm("movw  %0, %%ax;"
			"ltr %%ax;" : : "r" (seg) : "eax");
		tss_loaded[curr_cpu] = 1;
	}

	tss[curr_cpu]->rsp0 = kstack;
//	dprintf("done\n");
}

void reboot(void)
{
	static uint16 null_idt_descr[3];

//	asm("lidt	%0" : : "m" (null_idt_descr));
	asm("int 	$0");
	for(;;);
}

void arch_cpu_invalidate_TLB_range(addr_t start, addr_t end)
{
	for(; start < end; start += PAGE_SIZE) {
		invalidate_TLB(start);
	}
}

void arch_cpu_invalidate_TLB_list(addr_t pages[], int num_pages)
{
	int i;
	for(i=0; i<num_pages; i++) {
		invalidate_TLB(pages[i]);
	}
}

int arch_cpu_user_memcpy(void *to, const void *from, size_t size, addr_t *fault_handler)
{
	char *tmp = (char *)to;
	char *s = (char *)from;

	*fault_handler = (addr_t)&&error;

	while(size--)
		*tmp++ = *s++;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_strcpy(char *to, const char *from, addr_t *fault_handler)
{
	*fault_handler = (addr_t)&&error;

	while((*to++ = *from++) != '\0')
		;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_strncpy(char *to, const char *from, size_t size, addr_t *fault_handler)
{
	*fault_handler = (addr_t)&&error;

	while(size-- && (*to++ = *from++) != '\0')
		;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_memset(void *s, char c, size_t count, addr_t *fault_handler)
{
	char *xs = (char *) s;

	*fault_handler = (addr_t)&&error;

	while (count--)
		*xs++ = c;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

void arch_cpu_idle(void)
{
	switch(smp_get_num_cpus()) {
		case 0:
			panic("You need at least 1 CPU to run NewOS\n");
		case 1:
			asm("hlt");
		default:
			break;
	}
}

void arch_cpu_sync_icache(void *address, size_t len)
{
	// instruction cache is always consistent on x86
}

static void dbg_in(int argc, char **argv)
{
	int value;
	int port;

	if(argc < 2) {
		dprintf("not enough args\nusage: %s (1|2|4) port\n", argv[0]);
		return;
	}

	port = atoul(argv[2]);

	switch(argv[1][0]) {
		case '1':
		case 'b':
			value = in8(port);
			break;
		case '2':
		case 'h':
			value = in16(port);
			break;
		case '4':
		case 'w':
			value = in32(port);
			break;
		default:
			dprintf("invalid width argument\n");
			return;
	}
	dprintf("I/O port 0x%x = 0x%x\n", port, value);
}

static void dbg_out(int argc, char **argv)
{
	int value;
	int port;

	if(argc < 3) {
		dprintf("not enough args\nusage: %s (1|2|4) port value\n", argv[0]);
		return;
	}

	port = atoul(argv[2]);
	value = atoul(argv[3]);

	switch(argv[1][0]) {
		case '1':
		case 'b':
			out8(value, port);
			break;
		case '2':
		case 'h':
			out16(value, port);
			break;
		case '4':
		case 'w':
			out32(value, port);
			break;
		default:
			dprintf("invalid width argument\n");
			return;
	}
	dprintf("writing 0x%x to I/O port 0x%x\n", value, port);
}

