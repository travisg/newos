/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/cpu.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <sys/errors.h>

#include <boot/stage2.h>

#include <libc/string.h>
#include <libc/printf.h>

static struct tss **tss;
static int *tss_loaded;

static unsigned int *gdt = 0;

int arch_cpu_init(kernel_args *ka)
{
	setup_system_time(ka->arch_args.system_time_cv_factor);

	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	region_id rid;
	struct tss_descriptor *tss_d;
	unsigned int i;
	
	// account for the segment descriptors
	gdt = (unsigned int *)ka->arch_args.vir_gdt;	
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "gdt", (void **)&gdt,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
	
	tss = kmalloc(sizeof(struct tss *) * ka->num_cpus);
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
		
		memset(tss[i], 0, sizeof(struct tss));
		tss[i]->ss0 = KERNEL_DATA_SEG;
	
		// add TSS descriptor for this new TSS
		tss_d = (struct tss_descriptor *)&gdt[10 + i*2];
		tss_d->limit_00_15 = sizeof(struct tss) & 0xffff;
		tss_d->limit_19_16 = 0; // not this long
		tss_d->base_00_15 = (addr)tss[i] & 0xffff;
		tss_d->base_23_16 = ((addr)tss[i] >> 16) & 0xff;
		tss_d->base_31_24 = (addr)tss[i] >> 24;
		tss_d->type = 0x9;
		tss_d->zero = 0;
		tss_d->dpl = 0;
		tss_d->present = 1;
		tss_d->avail = 0;
		tss_d->zero1 = 0;
		tss_d->zero2 = 1;
		tss_d->granularity = 1;
	}
	return 0;
}

void i386_set_kstack(addr kstack)
{
	int curr_cpu = smp_get_current_cpu();

//	dprintf("i386_set_kstack: kstack 0x%x, cpu %d\n", kstack, curr_cpu);
	if(tss_loaded[curr_cpu] == 0) {
		short seg = (0x28 + 8*curr_cpu);
		asm("movw  %0, %%ax;"
			"ltr %%ax;" : : "r" (seg) : "eax");
		tss_loaded[curr_cpu] = 1;
	}

	tss[curr_cpu]->sp0 = kstack;
//	dprintf("done\n");
}

void arch_cpu_invalidate_TLB_range(addr start, addr end)
{
	for(; start < end; start += PAGE_SIZE) {
		invalidate_TLB(start);
	}
}

void arch_cpu_invalidate_TLB_list(addr pages[], int num_pages)
{
	int i;
	for(i=0; i<num_pages; i++) {
		invalidate_TLB(pages[i]);
	}
}
