#ifndef _STAGE2_H
#define _STAGE2_H

#include <boot.h>

// must match SMP_MAX_CPUS in arch_smp.h
#define MAX_BOOT_CPUS 4
#define MAX_PHYS_MEM_ADDR_RANGE 4
#define MAX_VIRT_ALLOC_ADDR_RANGE 4
#define MAX_PHYS_ALLOC_ADDR_RANGE 4

#define MAX_BOOT_PTABLES 4

typedef struct {
	unsigned long start;
	unsigned long size;
} addr_range;

// kernel args
typedef struct {
        unsigned int cons_line;
        char *str;
        addr_range bootdir_addr;
        addr_range kernel_seg0_addr;    
        addr_range kernel_seg1_addr;    
        unsigned int num_phys_mem_ranges;
        addr_range phys_mem_range[MAX_PHYS_MEM_ADDR_RANGE];
        unsigned int num_phys_alloc_ranges;
        addr_range phys_alloc_range[MAX_PHYS_ALLOC_ADDR_RANGE];
        unsigned int num_virt_alloc_ranges;
        addr_range virt_alloc_range[MAX_VIRT_ALLOC_ADDR_RANGE];
        unsigned int num_cpus;
        addr_range cpu_kstack[MAX_BOOT_CPUS];
	// architecture specific
	unsigned int system_time_cv_factor;
	unsigned int phys_pgdir;
	unsigned int vir_pgdir;
	unsigned int num_pgtables;
	unsigned int pgtables[MAX_BOOT_PTABLES];
	unsigned int phys_idt;
	unsigned int vir_idt;
	unsigned int phys_gdt;
	unsigned int vir_gdt;
	unsigned int page_hole;
	// smp stuff
	unsigned int apic_time_cv_factor; // apic ticks per second
	unsigned int apic_phys;
	unsigned int *apic;
	unsigned int ioapic_phys;
	unsigned int *ioapic;
	unsigned int cpu_apic_id[MAX_BOOT_CPUS];
	unsigned int cpu_os_id[MAX_BOOT_CPUS];
	unsigned int cpu_apic_version[MAX_BOOT_CPUS];
} kernel_args;

#endif

