#include <boot/stage2.h>
#include <arch/sh4/vcpu.h>
#include <arch/sh4/sh4.h>

#include <kernel/debug.h>
#include <kernel/int.h>
#include <kernel/thread.h>
#include <kernel/vm_priv.h>
#include <kernel/faults_priv.h>

#include <kernel/arch/sh4/cpu.h>

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

	// NOTE: not safe to do anything that may involve the FPU before 
	// it is certain it is not an fpu exception

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
		case 9: { // FPU exception
			int fpscr = get_fpscr();	
			int fpu_fault_code;
			switch(fpscr & 0x0003f000) {
				case 0x1000:
					fpu_fault_code = FPU_FAULT_CODE_INEXACT;	
					break;
				case 0x2000:
					fpu_fault_code = FPU_FAULT_CODE_UNDERFLOW;
					break;
				case 0x4000:
					fpu_fault_code = FPU_FAULT_CODE_OVERFLOW;
					break;
				case 0x8000:
					fpu_fault_code = FPU_FAULT_CODE_DIVBYZERO;
					break;
				case 0x10000:
					fpu_fault_code = FPU_FAULT_CODE_INVALID_OP;
					break;
				case 0x20000:
					fpu_fault_code = FPU_FAULT_CODE_UNKNOWN;
					break;
				default:
					// XXX handle better
					fpu_fault_code = FPU_FAULT_CODE_UNKNOWN;
			}
			ret = fpu_fault(fpu_fault_code);
			break;
		}
		case 64: // FPU disable exception
		case 65: // Slot FPU disable exception
			ret = fpu_disable_fault();
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

	vector_table = (struct vector *)ka->arch_args.vcpu->vt;
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

