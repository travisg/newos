/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <boot/stage2.h>

static vcpu_struct *vcpu;

int arch_cpu_init(kernel_args *ka)
{
	vcpu = ka->arch_args.vcpu;
	
	vcpu->kernel_asid = 0;
	vcpu->user_asid = 0;

	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	return 0;
}

void sh4_set_kstack(addr kstack)
{
//	dprintf("sh4_set_kstack: setting kstack to 0x%x\n", kstack);
	vcpu->kstack = (unsigned int *)kstack;
}

void sh4_set_user_pgdir(addr pgdir)
{
//	dprintf("sh4_set_user_pgdir: setting pgdir to 0x%x\n", pgdir);
	vcpu->user_pgdir = (unsigned int *)pgdir;
}
