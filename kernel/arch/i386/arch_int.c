#include "string.h"
#include "vm.h"
#include "debug.h"
#include "console.h"
#include "int.h"
#include "thread.h"

#include "arch_cpu.h"
#include "arch_interrupts.h"
#include "arch_int.h"
#include "arch_faults.h"
#include "arch_vm.h"
#include "arch_smp.h"

#include "stage2.h"

struct int_frame {
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int esp;
	unsigned int ebx;
	unsigned int edx;
	unsigned int ecx;
	unsigned int eax;
	unsigned int vector;
	unsigned int error_code;
	unsigned int eip;
	unsigned int cs;
	unsigned int flags;
	unsigned int user_esp;
	unsigned int user_ss;
};

desc_table *idt = NULL;


void interrupt_ack(int n)
{
	n -= 0x20;
	if(n < 0x10) {
		// 8239 controlled interrupt
		if(n > 7)
			outb(0x20, 0xa0);	// EOI to pic 2	
		outb(0x20, 0x20);	// EOI to pic 1
	}
}

static void _set_gate(desc_table *gate_addr, unsigned int addr, int type, int dpl)
{
	unsigned int gate1; // first byte of gate desc
	unsigned int gate2; // second byte of gate desc
	
	gate1 = (KERNEL_CODE_SEG << 16) | (0x0000ffff & addr);
	gate2 = (0xffff0000 & addr) | 0x8000 | (dpl << 13) | (type << 8);
	
	gate_addr->a = gate1;
	gate_addr->b = gate2;
}

void arch_int_enable_io_interrupt(int irq)
{
	// if this is a external interrupt via 8239, enable it here
	if (irq < 8)
		outb(inb(0x21) & ~(1 << irq), 0x21);
	else
		outb(inb(0xa1) & ~(1 << (irq - 8)), 0xa1);
}

void arch_int_disable_io_interrupt(int irq)
{
	// if this is a external interrupt via 8239, disable it here
	if (irq < 8)
		outb(inb(0x21) | (1 << irq), 0x21);
	else
		outb(inb(0xa1) | (1 << (irq - 8)), 0xa1);
}

static void set_intr_gate(int n, void *addr)
{
	_set_gate(&idt[n], (unsigned int)addr, 14, 0);
}

static void set_trap_gate(int n, void *addr)
{
	_set_gate(&idt[n], (unsigned int)addr, 15, 0);
}

static void set_system_gate(int n, void *addr)
{
	_set_gate(&idt[n], (unsigned int)addr, 15, 3);
}

void arch_int_enable_interrupts()
{
	asm("sti");
}

int arch_int_disable_interrupts()
{
	int flags;
	
	asm("pushfl;
		popl %0;
		cli" : "=g" (flags));
	return flags & 0x200 ? 1 : 0;
}

void arch_int_restore_interrupts(int oldstate)
{
	int flags = oldstate ? 0x200 : 0;
	int state = 0;

	asm volatile("pushfl;
		popl	%1;
		andl	$0xfffffdff,%1;
		orl		%0,%1;
		pushl	%1;
		popfl"
		: "=g" (flags) : "r" (state), "0" (flags));
}

void i386_handle_trap(struct int_frame frame)
{
	int ret;

	switch(frame.vector) {
		case 8:
			ret = i386_double_fault(frame.error_code);
			break;
		case 13:
			ret = i386_general_protection_fault(frame.error_code);
			break;
		case 14: {
			unsigned int cr2;
		
			asm volatile("movl %%cr2, %0;" : "=g" (cr2));
			ret = i386_page_fault(cr2, frame.eip);
			break;
		}
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			interrupt_ack(frame.vector); // ack the 8239
			ret = int_io_interrupt_handler(frame.vector - 0x20);
			break;
		case 0x254: case 0x255:
			ret = i386_smp_interrupt(frame.vector);
			break;
		default:
			kprintf("unhandled interrupt %d\n", frame.vector);
			ret = INT_NO_RESCHEDULE;
	}
	
	if(ret == INT_RESCHEDULE) {
		thread_resched();
	}	
}

int arch_int_init(struct kernel_args *ka)
{
	idt = (desc_table *)ka->vir_idt;

	// setup the interrupt controller
	outb(0x11, 0x20);	// Start initialization sequence for #1.
	outb(0x11, 0xa0);	// ...and #2.
	outb(0x20, 0x21);	// Set start of interrupts for #1 (0x20).
	outb(0x28, 0xa1);	// Set start of interrupts for #2 (0x28).
	outb(0x04, 0x21);	// Set #1 to be the master.
	outb(0x02, 0xa1);	// Set #2 to be the slave.
	outb(0x01, 0x21);	// Set both to operate in 8086 mode.
	outb(0x01, 0xa1);
	outb(0xfb, 0x21);	// Mask off all interrupts (except slave pic line).
	outb(0xff, 0xa1); 	// Mask off interrupts on the slave.

	set_intr_gate(0,  &trap0);
	set_intr_gate(1,  &trap1);
	set_intr_gate(2,  &trap2);
	set_intr_gate(3,  &trap3);
	set_intr_gate(4,  &trap4);
	set_intr_gate(5,  &trap5);
	set_intr_gate(6,  &trap6);
	set_intr_gate(7,  &trap7);
	set_intr_gate(8,  &trap8);
	set_intr_gate(9,  &trap9);
	set_intr_gate(10,  &trap10);
	set_intr_gate(11,  &trap11);
	set_intr_gate(12,  &trap12);
	set_intr_gate(13,  &trap13);
	set_intr_gate(14,  &trap14);
//	set_intr_gate(15,  &trap15);
	set_intr_gate(16,  &trap16);
	set_intr_gate(17,  &trap17);
	set_intr_gate(18,  &trap18);

	set_intr_gate(32,  &trap32);
	set_intr_gate(33,  &trap33);
	set_intr_gate(34,  &trap34);
	set_intr_gate(35,  &trap35);
	set_intr_gate(36,  &trap36);
	set_intr_gate(37,  &trap37);
	set_intr_gate(38,  &trap38);
	set_intr_gate(39,  &trap39);
	set_intr_gate(40,  &trap40);
	set_intr_gate(41,  &trap41);
	set_intr_gate(42,  &trap42);
	set_intr_gate(43,  &trap43);
	set_intr_gate(44,  &trap44);
	set_intr_gate(45,  &trap45);
	set_intr_gate(46,  &trap46);
	set_intr_gate(47,  &trap47);

	set_intr_gate(254, &trap254);
	set_intr_gate(255, &trap255);

	return 0;
}

int arch_int_init2(struct kernel_args *ka)
{
	idt = (desc_table *)ka->vir_idt;
	vm_create_area(vm_get_kernel_aspace(), "idt", (void *)&idt, AREA_ALREADY_MAPPED, PAGE_SIZE, 0);
	return 0;
}
