/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_BOOT_ARCH_I386_STAGE2_H
#define _NEWOS_KERNEL_BOOT_ARCH_I386_STAGE2_H

#include <boot/stage2_struct.h>
#include <newos/compiler.h>

#define MAX_BOOT_PTABLES 16

#define IDT_LIMIT 0x800
#define GDT_LIMIT 0x800

struct gdt_idt_descr {
    unsigned short a;
    unsigned int *b;
} _PACKED;


// kernel args
typedef struct {
    // architecture specific
    unsigned int system_time_cv_factor;
    addr_t phys_pgdir;
    unsigned int num_pgtables;
    addr_t pgtables[MAX_BOOT_PTABLES];
    addr_t phys_idt;
    addr_t vir_idt;
    addr_t phys_gdt;
    addr_t vir_gdt;

    // smp stuff
    unsigned int apic_time_cv_factor; // apic ticks per second
    addr_t apic_phys;
    unsigned int *apic;
    addr_t ioapic_phys;
    unsigned int *ioapic;
    unsigned int cpu_apic_id[_MAX_CPUS];
    unsigned int cpu_os_id[_MAX_CPUS];
    unsigned int cpu_apic_version[_MAX_CPUS];
} arch_kernel_args;

#endif

