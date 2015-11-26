/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_I386_CPU_H
#define _NEWOS_KERNEL_ARCH_I386_CPU_H

#include <arch/cpu.h>
#include <kernel/arch/i386/thread_struct.h>
#include <kernel/arch/i386/descriptors.h>

typedef struct desc_struct {
    unsigned int a,b;
} desc_table;

struct tss {
    uint16 prev_task;
    uint16 unused0;
    uint32 sp0;
    uint32 ss0;
    uint32 sp1;
    uint32 ss1;
    uint32 sp2;
    uint32 ss2;
    uint32 sp3;
    uint32 ss3;
    uint32 cr3;
    uint32 eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32 es, cs, ss, ds, fs, gs;
    uint32 ldt_seg_selector;
    uint16 unused1;
    uint16 io_map_base;
};

struct tss_descriptor {
    uint16 limit_00_15;
    uint16 base_00_15;
    uint32 base_23_16 : 8;
    uint32 type : 4;
    uint32 zero : 1;
    uint32 dpl : 2;
    uint32 present : 1;
    uint32 limit_19_16 : 4;
    uint32 avail : 1;
    uint32 zero1 : 1;
    uint32 zero2 : 1;
    uint32 granularity : 1;
    uint32 base_31_24 : 8;
};

typedef struct ptentry {
    unsigned int present:1;
    unsigned int rw:1;
    unsigned int user:1;
    unsigned int write_through:1;
    unsigned int cache_disabled:1;
    unsigned int accessed:1;
    unsigned int dirty:1;
    unsigned int reserved:1;
    unsigned int global:1;
    unsigned int avail:3;
    unsigned int addr:20;
} ptentry;

typedef struct pdentry {
    unsigned int present:1;
    unsigned int rw:1;
    unsigned int user:1;
    unsigned int write_through:1;
    unsigned int cache_disabled:1;
    unsigned int accessed:1;
    unsigned int reserved:1;
    unsigned int page_size:1;
    unsigned int global:1;
    unsigned int avail:3;
    unsigned int addr:20;
} pdentry;

struct iframe {
    unsigned int gs;
    unsigned int fs;
    unsigned int es;
    unsigned int ds;
    unsigned int edi;       /* 0x10 */
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;       /* 0x20 */
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int orig_eax;  /* 0x30 */
    unsigned int orig_edx;
    unsigned int vector;
    unsigned int error_code;
    unsigned int eip;       /* 0x40 */
    unsigned int cs;
    unsigned int flags;
    unsigned int user_esp;
    unsigned int user_ss;   /* 0x50 */
};

// x86 features from cpuid eax 1, ecx register
#define X86_FPU   0x00000001 // x87 fpu
#define X86_VME   0x00000002 // virtual 8086
#define X86_DE    0x00000004 // debugging extensions
#define X86_PSE   0x00000008 // page size extensions
#define X86_TSC   0x00000010 // rdtsc instruction
#define X86_MSR   0x00000020 // rdmsr/wrmsr instruction
#define X86_PAE   0x00000040 // extended 3 level page table addressing
#define X86_MCE   0x00000080 // machine check exception
#define X86_CX8   0x00000100 // cmpxchg8b instruction
#define X86_APIC  0x00000200 // local apic on chip
#define X86_SEP   0x00000800 // SYSENTER/SYSEXIT
#define X86_MTRR  0x00001000 // MTRR
#define X86_PGE   0x00002000 // paging global bit
#define X86_MCA   0x00004000 // machine check architecture
#define X86_CMOV  0x00008000 // cmov instruction
#define X86_PAT   0x00010000 // page attribute table
#define X86_PSE36 0x00020000 // page size extensions with 4MB pages
#define X86_PSN   0x00040000 // processor serial number
#define X86_CLFSH 0x00080000 // cflush instruction
#define X86_DS    0x00200000 // debug store
#define X86_ACPI  0x00400000 // thermal monitor and clock ctrl
#define X86_MMX   0x00800000 // mmx instructions
#define X86_FXSR  0x01000000 // FXSAVE/FXRSTOR instruction
#define X86_SSE   0x02000000 // SSE
#define X86_SSE2  0x04000000 // SSE2
#define X86_SS    0x08000000 // self snoop
#define X86_HTT   0x10000000 // hyperthreading
#define X86_TM    0x20000000 // thermal monitor
#define X86_PBE   0x80000000 // pending break enable

// x86 features from cpuid eax 1, edx register
#define X86_EXT_SSE3  0x00000001 // SSE3
#define X86_EXT_MONITOR 0x00000008 // MONITOR/MWAIT
#define X86_EXT_DSCPL 0x00000010 // CPL qualified debug store
#define X86_EXT_EST   0x00000080 // speedstep
#define X86_EXT_TM2   0x00000100 // thermal monitor 2
#define X86_EXT_CNXTID 0x00000400 // L1 context ID

// x86 features from cpuid eax 0x80000001, edx register (AMD)
// only care about the ones that are unique to this register
#define X86_AMD_EXT_SYSCALL 0x00000800 // SYSCALL/SYSRET
#define X86_AMD_EXT_NX      (1<<20)    // no execute bit
#define X86_AMD_EXT_MMXEXT  (1<<22)    // mmx extensions
#define X86_AMD_EXT_FFXSR   (1<<25)    // fast FXSAVE/FXRSTOR
#define X86_AMD_EXT_LONG    (1<<29)    // long mode
#define X86_AMD_EXT_3DNOWEXT (1<<30)   // 3DNow! extensions
#define X86_AMD_EXT_3DNOW   (1<<31)   // 3DNow!

// features
enum i386_feature_type {
    FEATURE_COMMON = 0,     // cpuid eax=1, ecx register
    FEATURE_EXT,            // cpuid eax=1, edx register
    FEATURE_EXT_AMD,        // cpuid eax=0x80000001, edx register (AMD)

    FEATURE_NUM
};

enum i386_vendors {
    VENDOR_INTEL = 0,
    VENDOR_AMD,
    VENDOR_CYRIX,
    VENDOR_UMC,
    VENDOR_NEXGEN,
    VENDOR_CENTAUR,
    VENDOR_RISE,
    VENDOR_TRANSMETA,
    VENDOR_NSC,

    VENDOR_NUM,
    VENDOR_UNKNOWN,
};

struct arch_cpu_info {
    enum i386_vendors vendor;
    enum i386_feature_type feature[FEATURE_NUM];
    char model_name[49];
    const char *vendor_name;
    char feature_string[512];
    int family;
    int stepping;
    int model;
};

#define nop() __asm__ ("nop"::)

void setup_system_time(unsigned int cv_factor);
bigtime_t i386_cycles_to_time(uint64 cycles);
void i386_context_switch(struct arch_thread *old, struct arch_thread *new);
void i386_enter_uspace(addr_t entry, void *args, addr_t ustack_top);
void i386_set_kstack(addr_t kstack);
void i386_switch_stack_and_call(addr_t stack, void (*func)(void *), void *arg);
void i386_swap_pgdir(addr_t new_pgdir);
void i386_fsave(void *fpu_state);
void i386_fxsave(void *fpu_state);
void i386_frstor(void *fpu_state);
void i386_fxrstor(void *fpu_state);
void i386_fsave_swap(void *old_fpu_state, void *new_fpu_state);
void i386_fxsave_swap(void *old_fpu_state, void *new_fpu_state);
void i386_save_fpu_context(void *fpu_state);
void i386_load_fpu_context(void *fpu_state);
void i386_swap_fpu_context(void *old_fpu_state, void *new_fpu_state);
desc_table *i386_get_gdt(void);
void i386_set_task_gate(int n, uint32 seg);
uint64 i386_rdtsc(void);
void i386_cpuid(unsigned int selector, unsigned int *data);
bool i386_check_feature(uint32 feature, enum i386_feature_type type);
void i386_set_task_switched(void);
void i386_clear_task_switched(void);

#define read_cr0(value) \
    __asm__("movl	%%cr0,%0" : "=r" (value))

#define write_cr0(value) \
    __asm__("movl	%0,%%cr0" :: "r" (value))

#define read_cr2(value) \
    __asm__("movl	%%cr2,%0" : "=r" (value))

#define write_cr2(value) \
    __asm__("movl	%0,%%cr2" :: "r" (value))

#define read_cr3(value) \
    __asm__("movl	%%cr3,%0" : "=r" (value))

#define write_cr3(value) \
    __asm__("movl	%0,%%cr3" :: "r" (value))

#define read_cr4(value) \
    __asm__("movl	%%cr4,%0" : "=r" (value))

#define write_cr4(value) \
    __asm__("movl	%0,%%cr4" :: "r" (value))

#define read_ebp(value) \
    __asm__("movl	%%ebp,%0" : "=r" (value))

#define read_dr3(value) \
    __asm__("movl	%%dr3,%0" : "=r" (value))

#define write_dr3(value) \
    __asm__("movl	%0,%%dr3" :: "r" (value))

#define invalidate_TLB(va) \
    __asm__("invlpg (%0)" : : "r" (va))

#define out8(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

#define out16(value,port) \
__asm__ ("outw %%ax,%%dx"::"a" (value),"d" (port))

#define out32(value,port) \
__asm__ ("outl %%eax,%%dx"::"a" (value),"d" (port))

#define in8(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})

#define in16(port) ({ \
unsigned short _v; \
__asm__ volatile ("inw %%dx,%%ax":"=a" (_v):"d" (port)); \
_v; \
})

#define in32(port) ({ \
unsigned int _v; \
__asm__ volatile ("inl %%dx,%%eax":"=a" (_v):"d" (port)); \
_v; \
})

#define out8_p(value,port) \
__asm__ ("outb %%al,%%dx\n" \
        "\tjmp 1f\n" \
        "1:\tjmp 1f\n" \
        "1:"::"a" (value),"d" (port))

#define in8_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
    "\tjmp 1f\n" \
    "1:\tjmp 1f\n" \
    "1:":"=a" (_v):"d" (port)); \
_v; \
})

#endif

