#ifndef _STAGE2_H
#define _STAGE2_H

#include <boot.h>

// must match SMP_MAX_CPUS in arch_smp.h
#define MAX_BOOT_CPUS 1
#define MAX_PHYS_MEM_ADDR_RANGE 1
#define MAX_VIRT_ALLOC_ADDR_RANGE 2
#define MAX_PHYS_ALLOC_ADDR_RANGE 2

typedef struct {
	unsigned int start;
	unsigned int size;
} addr_range;

#include <vcpu_struct.h>

// kernel args
typedef struct {
	unsigned int cons_line;
	char *str;
	const boot_entry *bootdir;
	unsigned int bootdir_size;
	unsigned int kernel_seg0_base;
	unsigned int kernel_seg0_size;
	unsigned int kernel_seg1_base;
	unsigned int kernel_seg1_size;
	addr_range phys_mem_range[MAX_PHYS_MEM_ADDR_RANGE];
	addr_range phys_alloc_range[MAX_PHYS_ALLOC_ADDR_RANGE];
	addr_range virt_alloc_range[MAX_VIRT_ALLOC_ADDR_RANGE];
	unsigned int num_cpus;
	addr_range cpu_kstack[MAX_BOOT_CPUS];
	// architecture specific
	vcpu_struct *vcpu; 
} kernel_args;

#endif

