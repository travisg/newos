/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_X86_64_CPU_H
#define _NEWOS_KERNEL_ARCH_X86_64_CPU_H

#include <newos/compiler.h>
#include <arch/cpu.h>
#include <kernel/arch/x86_64/thread_struct.h>
#include <kernel/arch/x86_64/descriptors.h>

typedef struct desc_struct {
	unsigned long a,b;
} desc_table;

/* 64-bit TSS */
struct tss_64 {
	uint32 reserved;
	uint64 rsp0;
	uint64 rsp1;
	uint64 rsp2;
	uint64 reserved2;
	uint64 ist1;
	uint64 ist2;
	uint64 ist3;
	uint64 ist4;
	uint64 ist5;
	uint64 ist6;
	uint64 ist7;
	uint64 reserved3;
	uint32 reserved4;
	uint32 io_map_base;
} _PACKED;	

/* non system descriptors */
struct descriptor_32 {
	uint16 limit_00_15;
	uint16 base_00_15;
	uint32 base_23_16 : 8;
	uint32 type : 4;
	uint32 system : 1;
	uint32 dpl : 2;
	uint32 present : 1;
	uint32 limit_19_16 : 4;
	uint32 avail : 1;
	uint32 long_mode : 1;
	uint32 def_size : 1;
	uint32 granularity : 1;
	uint32 base_31_24 : 8;
} _PACKED;

/* system descriptors */
struct descriptor_64 {
	uint16 limit_00_15;
	uint16 base_00_15;
	uint32 base_23_16 : 8;
	uint32 type : 4;
	uint32 system : 1;
	uint32 dpl : 2;
	uint32 present : 1;
	uint32 limit_19_16 : 4;
	uint32 zero : 2;
	uint32 granularity : 1;
	uint32 base_31_24 : 8;
	uint32 base_63_32;
	uint32 reserved : 8;
	uint32 zero2 : 5;
	uint32 reserved2 : 19;
} _PACKED;

/* page table entry, same for all four levels (some bits ignored at various levels) */
typedef struct ptentry {
	unsigned long present:1;
	unsigned long rw:1;
	unsigned long user:1;
	unsigned long write_through:1;
	unsigned long cache_disabled:1;
	unsigned long accessed:1;
	unsigned long dirty:1;
	unsigned long reserved:1;
	unsigned long global:1;
	unsigned long avail:3;
	unsigned long addr:51;	
	unsigned long no_execute:1;
} ptentry;

struct iframe {
	unsigned long gs;
	unsigned long fs;
	unsigned long es;
	unsigned long ds;
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long rdi;
	unsigned long rsi;
	unsigned long rbp;
	unsigned long rsp;
	unsigned long rbx;
	unsigned long rdx;
	unsigned long rcx;
	unsigned long rax;
	unsigned long vector;
	unsigned long error_code;
	unsigned long rip;
	unsigned long cs;
	unsigned long flags;
	unsigned long user_sp;
	unsigned long user_ss;
};

struct arch_cpu_info {
	// empty
};

#define nop() __asm__ ("nop"::)

void setup_system_time(unsigned int cv_factor);
bigtime_t x86_64_cycles_to_time(uint64 cycles);
void x86_64_context_switch(addr_t *old_sp, addr_t new_sp, addr_t new_pgdir);
void x86_64_enter_uspace(addr_t entry, void *args, addr_t ustack_top);
void x86_64_set_kstack(addr_t kstack);
void x86_64_switch_stack_and_call(addr_t stack, void (*func)(void *), void *arg);
void x86_64_swap_pgdir(addr_t new_pgdir);
void x86_64_fsave(void *fpu_state);
void x86_64_fxsave(void *fpu_state);
void x86_64_frstor(void *fpu_state);
void x86_64_fxrstor(void *fpu_state);
void x86_64_fsave_swap(void *old_fpu_state, void *new_fpu_state);
void x86_64_fxsave_swap(void *old_fpu_state, void *new_fpu_state);
desc_table *x86_64_get_gdt(void);
uint64 x86_64_rdtsc(void);

addr_t read_cr3(void);
extern inline addr_t read_cr3(void) {
	addr_t val;
	__asm__("mov	%%cr3,%0" : "=r" (val));
	return val;
}

addr_t read_rbp(void);
extern inline addr_t read_rbp(void) {
	addr_t val;
	__asm__("mov	%%rbp,%0" : "=r" (val));
	return val;
}

addr_t read_dr3(void);
extern inline addr_t read_dr3(void) {
	addr_t val;
	__asm__("mov	%%dr3,%0" : "=r" (val));
	return val;
}

void write_dr3(addr_t val);
extern inline void write_dr3(addr_t val) {
	__asm__("mov	%0,%%dr3" :: "r" (val));
}

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

