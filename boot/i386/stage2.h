#ifndef _STAGE2_H
#define _STAGE2_H

#include "boot.h"

// must match SMP_MAX_CPUS in arch_smp.h
#define MAX_BOOT_CPUS 2
#define MAX_BOOT_PTABLES 4

// kernel args
typedef struct {
	unsigned int cons_line;
	unsigned int mem_size;
	char *str;
	const boot_entry *bootdir;
	unsigned int bootdir_size;
	unsigned int kernel_seg0_base;
	unsigned int kernel_seg0_size;
	unsigned int kernel_seg1_base;
	unsigned int kernel_seg1_size;
	unsigned int phys_alloc_range_low;
	unsigned int phys_alloc_range_high;
	unsigned int virt_alloc_range_low;
	unsigned int virt_alloc_range_high;
	unsigned int num_cpus;
	unsigned int cpu_kstack[MAX_BOOT_CPUS];
	unsigned int cpu_kstack_len[MAX_BOOT_CPUS];
	// architecture specific
	unsigned int system_time_cv_factor;
	unsigned int pgdir;
	unsigned int num_pgtables;
	unsigned int pgtables[MAX_BOOT_PTABLES];
	unsigned int phys_idt;
	unsigned int vir_idt;
	unsigned int phys_gdt;
	unsigned int vir_gdt;
	unsigned int page_hole;
	// smp stuff
	unsigned int apic_phys;
	unsigned int *apic;
	unsigned int ioapic_phys;
	unsigned int *ioapic;
	unsigned int cpu_apic_id[MAX_BOOT_CPUS];
	unsigned int cpu_os_id[MAX_BOOT_CPUS];
	unsigned int cpu_apic_version[MAX_BOOT_CPUS];
} kernel_args;

#endif

