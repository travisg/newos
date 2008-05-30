/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>
#include <kernel/thread.h>
#include <kernel/smp.h>
#include <kernel/syscalls.h>
#include <kernel/vm_priv.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>
#include <kernel/arch/faults.h>
#include <kernel/arch/vm.h>
#include <kernel/arch/smp.h>

#include <kernel/arch/x86_64/interrupts.h>
#include <kernel/arch/x86_64/faults.h>

#include <boot/stage2.h>

#include <string.h>

#define MAX_ARGS 16

#define SYSCALL_VECTOR 99

static trap_descriptor_64 *idt = NULL;

static void interrupt_ack(int n)
{
	if(n < 16) {
		// 8239 controlled interrupt
		if(n > 7)
			out8(0x20, 0xa0);	// EOI to pic 2
		out8(0x20, 0x20);	// EOI to pic 1
	}
}
void arch_int_enable_io_interrupt(int irq)
{
	if(irq > 15)
		return;

	// if this interrupt is >= 8, then enable the cascade interrupt
	if(irq >= 8)
		arch_int_enable_io_interrupt(2);

	// if this is a external interrupt via 8239, enable it here
	if (irq < 8)
		out8(in8(0x21) & ~(1 << irq), 0x21);
	else
		out8(in8(0xa1) & ~(1 << (irq - 8)), 0xa1);
}

void arch_int_disable_io_interrupt(int irq)
{
	if (irq > 15)
		return;

	// if this is a external interrupt via 8239, disable it here
	if (irq < 8)
		out8(in8(0x21) | (1 << irq), 0x21);
	else
		out8(in8(0xa1) | (1 << (irq - 8)), 0xa1);
}

static void _set_gate(trap_descriptor_64 *gate_addr, addr_t addr, int type, int dpl)
{
	gate_addr->a = (KERNEL_CODE_SEG << 16)| (0x0000ffff & addr);
	gate_addr->b = (addr & 0xffff0000) | (1<<15) | (dpl << 13) | (type << 8);
	gate_addr->c = (addr >> 32);
}

static void set_intr_gate(int n, void *addr)
{
	_set_gate(&idt[n], (addr_t)addr, 14, 0);
}

// unused
#if 0
static void set_trap_gate(int n, void *addr)
{
	_set_gate(&idt[n], (addr_t)addr, 15, 0);
}
#endif

static void set_system_gate(int n, void *addr)
{
	_set_gate(&idt[n], (addr_t)addr, 15, 3);
}

void arch_int_enable_interrupts(void)
{
	asm("sti");
}

void arch_int_disable_interrupts(void)
{
	asm("cli");
}

bool arch_int_are_interrupts_enabled(void)
{
	unsigned long flags;

	asm("pushf;\n"
		"pop %0;\n" : "=g" (flags));
	return flags & 0x200 ? 1 : 0;
}

void x86_64_handle_trap(struct iframe *frame); /* keep the compiler happy, this function must be called only from assembly */
void x86_64_handle_trap(struct iframe *frame)
{
	dprintf("%s: vector %ld, ip 0x%lx\n", __PRETTY_FUNCTION__, frame->vector, frame->rip);

	int ret = INT_NO_RESCHEDULE;
	struct thread *t = thread_get_current_thread();

//	dprintf("t %p\n", t);
	
	if(t) {
		x86_64_push_iframe(frame);
		t->int_disable_level++; // make it look like the ints were disabled
	}

//	if(frame.vector != 0x20)
//		dprintf("x86_64_handle_trap: vector 0x%x, ip 0x%x, cpu %d\n", frame.vector, frame.eip, smp_get_current_cpu());
	switch(frame->vector) {
		case 8:
			ret = x86_64_double_fault(frame->error_code);
			break;
		case 13:
			ret = x86_64_general_protection_fault(frame->error_code);
			break;
		case 14: {
			addr_t cr2;
			addr_t newip;

			asm ("mov  %%cr2, %0" : "=r" (cr2) );

			if(!kernel_startup && (frame->flags & 0x200) == 0) {
				// ints are were disabled when the page fault was taken, that is very bad
				panic("x86_64_handle_trap: page fault at 0x%lx, ip 0x%lx, write %d with ints disabled\n",
					cr2, frame->rip, (frame->error_code & 0x2) != 0);
			}

			if(!kernel_startup) {
				int_restore_interrupts(); // should enable the interrupts
				ASSERT(int_are_interrupts_enabled());
			}


			ret = vm_page_fault(cr2, frame->rip,
				(frame->error_code & 0x2) != 0,
				(frame->error_code & 0x4) != 0,
				&newip);
			if(newip != 0) {
				// the page fault handler wants us to modify the iframe to set the
				// IP the cpu will return to to be this ip
				frame->rip = newip;
			}

			if(!kernel_startup)
				int_disable_interrupts();
			break;
		}
		default:
			if(frame->vector >= 0x20) {
				interrupt_ack(frame->vector - 0x20); // ack the 8239 (if applicable)
				ret = int_io_interrupt_handler(frame->vector - 0x20);
			} else {
				panic("x86_64_handle_trap: unhandled cpu trap 0x%lx at ip 0x%lx!\n", frame->vector, frame->rip);
				ret = INT_NO_RESCHEDULE;
			}
			break;
	}

	// try to deliver signals to the interrupted thread
	// XXX should we only do it for timer interrupts?
	if(frame->cs == USER_CODE_SEG)
		ret |= thread_atinterrupt_exit();
	if(ret == INT_RESCHEDULE) {
		GRAB_THREAD_LOCK();
		thread_resched();
		RELEASE_THREAD_LOCK();
	}

//	dprintf("0x%x cpu %d!\n", thread_get_current_thread_id(), smp_get_current_cpu());

	if(t) {
		x86_64_pop_iframe();
		t->int_disable_level--; // keep the count in sync
	}
}

int arch_int_init(kernel_args *ka)
{
	idt = (trap_descriptor_64 *)ka->arch_args.vir_idt;

	// setup the interrupt controller
	out8(0x11, 0x20);	// Start initialization sequence for #1.
	out8(0x11, 0xa0);	// ...and #2.
	out8(0x20, 0x21);	// Set start of interrupts for #1 (0x20).
	out8(0x28, 0xa1);	// Set start of interrupts for #2 (0x28).
	out8(0x04, 0x21);	// Set #1 to be the master.
	out8(0x02, 0xa1);	// Set #2 to be the slave.
	out8(0x01, 0x21);	// Set both to operate in 8086 mode.
	out8(0x01, 0xa1);
	out8(0xfb, 0x21);	// Mask off all interrupts (except slave pic line).
	out8(0xff, 0xa1); 	// Mask off interrupts on the slave.

	set_intr_gate(0,  &trap0);
	set_intr_gate(1,  &trap1);
	set_intr_gate(2,  &trap2);
	set_intr_gate(3,  &trap3);
	set_intr_gate(4,  &trap4);
	set_intr_gate(5,  &trap5);
	set_intr_gate(6,  &trap6);
	set_intr_gate(7,  &trap7);
//	set_intr_gate(8,  &trap8);	/* handled below by pointing the idt entry to a tss segment */
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

	set_system_gate(SYSCALL_VECTOR, &x86_64_syscall_vector);

	set_intr_gate(251, &trap251);
	set_intr_gate(252, &trap252);
	set_intr_gate(253, &trap253);
	set_intr_gate(254, &trap254);
	set_intr_gate(255, &trap255);

	return 0;
}

int arch_int_init2(kernel_args *ka)
{
	idt = (trap_descriptor_64 *)ka->arch_args.vir_idt;
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "idt", (void *)&idt,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	return 0;
}
