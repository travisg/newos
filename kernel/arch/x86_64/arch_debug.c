/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/elf.h>
#include <kernel/arch/debug.h>
#include <kernel/arch/x86_64/cpu.h>

static void dbg_stack_trace(int argc, char **argv)
{
	struct thread *t;
	addr_t rbp;
	int i;

	if(argc < 2)
		t = thread_get_current_thread();
	else {
		dprintf("not supported\n");
		return;
	}

	for(i=0; i<t->arch_info.iframe_ptr; i++) {
		char *temp = (char *)t->arch_info.iframes[i];
		dprintf("iframe %p %p %p\n", temp, temp + sizeof(struct iframe), temp + sizeof(struct iframe) - 8);
	}

	dprintf("stack trace for thread 0x%x '%s'\n", t->id, t->name);
	dprintf("frame     \tcaller:<base function+offset>\n");
	dprintf("-------------------------------\n");

	rbp = read_rbp();
	for(;;) {
		bool is_iframe = false;
		// see if the ebp matches the iframe
		for(i=0; i<t->arch_info.iframe_ptr; i++) {
			if(rbp == ((addr_t) t->arch_info.iframes[i] - 8)) {
				// it's an iframe
				is_iframe = true;
			}
		}

		if(is_iframe) {
			struct iframe *frame = (struct iframe *)(rbp + 8);
			dprintf("iframe at %p\n", frame);
			dprintf(" rax\t0x%lx\trbx\t0x%lx\trcx\t0x%lx\trdx\t0x%lx\n", frame->rax, frame->rbx, frame->rcx, frame->rdx);
			dprintf(" rsi\t0x%lx\trdi\t0x%lx\trbp\t0x%lx\n", frame->rsi, frame->rdi, frame->rbp);
			dprintf("  r8\t0x%lx\t r9\t0x%lx\tr10\t0x%lx\tr11\t0x%lx\n", frame->r8, frame->r9, frame->r10, frame->r11);
			dprintf(" r12\t0x%lx\tr13\t0x%lx\tr14\t0x%lx\tr15\t0x%lx\n", frame->r12, frame->r13, frame->r14, frame->r15);
			dprintf(" rip\t0x%lx\trflags\t0x%lx", frame->rip, frame->flags);
			if((frame->error_code & 0x4) != 0) {
				// from user space
				dprintf("\tuser esp\t0x%lx", frame->user_sp);
			}
			dprintf("\n");
			dprintf(" vector\t0x%lx\terror code\t0x%lx\n", frame->vector, frame->error_code);
 			rbp = frame->rbp;
		} else {
			addr_t rip = *((addr_t *)rbp + 1);
			char symname[256];
			addr_t base_address;

			if(rip == 0 || rbp == 0)
				break;

			if(elf_reverse_lookup_symbol(rip, &base_address, symname, sizeof(symname)) >= 0) {
				dprintf("0x%lx\t0x%lx:<0x%lx+0x%lx>\t'%s'\n", rbp, rip, base_address,
					(addr_t)(rip - base_address), symname);
			} else {
				dprintf("0x%lx\t0x%lx\n", rbp, rip);
			}
			rbp = *(addr_t *)rbp;
		}
	}
}

int arch_dbg_init(kernel_args *ka)
{
	return NO_ERROR;
}

int arch_dbg_init2(kernel_args *ka)
{
	// at this stage, the debugger command system is alive

	dbg_add_command(&dbg_stack_trace, "bt", "Stack crawl for current thread");

	return NO_ERROR;
}

void dbg_make_register_file(unsigned int *file, const struct iframe * frame)
{
#warning needs to match the actual x86_64 register file size
	file[0] = frame->rax;
	file[1] = frame->rbx;
	file[2] = frame->rcx;
	file[3] = frame->rdx;
	file[4] = frame->rsp;
	file[5] = frame->rbp;
	file[6] = frame->rsi;
	file[7] = frame->rdi;
	file[8] = frame->rip;
	file[9] = frame->flags;
	file[10] = frame->cs;
	file[11] = frame->user_ss;
	file[12] = 0;
	file[13] = 0;
}
