#include <stage2.h>
#include <vcpu.h>
#include <sh4.h>

#include <kernel/debug.h>
#include <kernel/int.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <kernel/faults.h>

struct vector *vector_table;

void arch_int_enable_io_interrupt(int irq)
{
	return;
}

void arch_int_disable_io_interrupt(int irq)
{
	return;
}

static int sh4_handle_exception(unsigned int code, unsigned int pc, unsigned int trap, unsigned int page_fault_addr)
{
	int ret;

//	dprintf("sh4_handle_exception: entry code 0x%x, pc 0x%x, trap 0x%x, page_fault_addr 0x%x\n", code, pc, trap, page_fault_addr);
	switch(code) {
		case 0:  // reset
		case 1:  // manual reset
		case 5:  // TLB protection violation (read)
		case 6:  // TLB protection violation (write)
		case 7:  // data address error (read)
		case 8:  // data address error (write)
		case 10: // TLB multi hit
		case 12: // illegal instruction
		case 13: // slot illegal instruction
			ret = general_protection_fault(code);
			break;
		case EXCEPTION_PAGE_FAULT:
			ret = vm_page_fault(page_fault_addr, pc);
			break;
		default:
			ret = int_io_interrupt_handler(code);
	}
        if(ret == INT_RESCHEDULE) {
                GRAB_THREAD_LOCK();
                thread_resched();
                RELEASE_THREAD_LOCK();
        } 

	return 0;
}

int arch_int_init(kernel_args *ka)
{
	int i;

	dprintf("arch_int_init: entry\n");

	vector_table = (struct vector *)ka->vcpu->vt;
	dprintf("arch_int_init: vector table 0x%x\n", vector_table);

	// set up the vectors
	// handle all of them
	for(i=0; i<256; i++)
		vector_table[i].func = &sh4_handle_exception;
	
	return 0;
}

int arch_int_init2(kernel_args *ka)
{
	return 0;
}

