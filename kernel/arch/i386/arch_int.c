#include "string.h"
#include "vm.h"
#include "debug.h"

#include "arch_cpu.h"
#include "arch_interrupts.h"
#include "arch_int.h"

#include "stage2.h"

desc_table *idt = NULL;

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
#if 0 // XXX not really needed	
	set_intr_gate(0x20, &_default_int0);
	set_intr_gate(0x21, &_default_int1);
	set_intr_gate(0x22, &_default_int2);
	set_intr_gate(0x23, &_default_int3);
	set_intr_gate(0x24, &_default_int4);
	set_intr_gate(0x25, &_default_int5);
	set_intr_gate(0x26, &_default_int6);
	set_intr_gate(0x27, &_default_int7);
	set_intr_gate(0x28, &_default_int8);
	set_intr_gate(0x29, &_default_int9);
	set_intr_gate(0x2a, &_default_int10);
	set_intr_gate(0x2b, &_default_int11);
	set_intr_gate(0x2c, &_default_int12);
	set_intr_gate(0x2d, &_default_int13);
	set_intr_gate(0x2e, &_default_int14);
	set_intr_gate(0x2f, &_default_int15);
#endif
	set_intr_gate(8, &_double_fault_int);
	set_intr_gate(13, &_general_protection_fault_int);

	return 0;
}

int arch_int_init2(struct kernel_args *ka)
{
	vm_map_physical_memory(vm_get_kernel_aspace(), "idt", (void *)&idt, AREA_ANY_ADDRESS, PAGE_SIZE, 0, ka->phys_idt);
	return 0;
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

void set_intr_gate(int n, void *addr)
{
	_set_gate(&idt[n], (unsigned int)addr, 14, 0);
	if(n >= 0x20 && n < 0x30) {
		int irq = n - 0x20;
		// if this is a external interrupt via 8239, enable it here
		if (irq < 8)
			outb(inb(0x21) & ~(1 << irq), 0x21);
		else
			outb(inb(0xa1) & ~(1 << (irq - 8)), 0xa1);
	}
}

void set_trap_gate(int n, void *addr)
{
	_set_gate(&idt[n], (unsigned int)addr, 15, 0);
}

void set_system_gate(int n, void *addr)
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

