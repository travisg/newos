#ifndef _I386_H
#define _I386_H

#include <kernel/kernel.h>
#include <stage2.h>

#define PAGE_ALIGN(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

#define KERNEL_CODE_SEG 0x8

typedef struct desc_struct {
	unsigned int a,b;
} desc_table;

#define nop() __asm__ ("nop"::)

int atomic_add(int *val, int incr);
int atomic_and(int *val, int incr);
int atomic_or(int *val, int incr);
int test_and_set(int *val, int set_to);

time_t system_time();
void setup_system_time(unsigned int cv_factor);
int arch_cpu_init(kernel_args *ka);
void i386_context_switch(unsigned int **old_esp, unsigned int *new_esp);
void reboot();

#define iret() __asm__ ("iret"::)

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

