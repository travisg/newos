/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>

#include <kernel/arch/vm.h>

int arch_vm_init(kernel_args *ka)
{
	return 0;
}

int arch_vm_init2(kernel_args *ka)
{
	int bats[8];
	int i;

#if 0
	// turn off any previous BAT mappings
	memset(bats, 0, sizeof(bats));
	setibats(bats);
	setdbats(bats);
#else
	// just clear the first BAT mapping (0 - 256MB)
	dprintf("msr 0x%x\n", getmsr());
	{
		unsigned int reg;
		asm("mr	%0,1" : "=r"(reg));
		dprintf("sp 0x%x\n", reg);
	}
	dprintf("ka %p\n", ka);

	getibats(bats);
	dprintf("ibats:\n");
	for(i = 0; i < 4; i++)
		dprintf("0x%x 0x%x\n", bats[i*2], bats[i*2+1]);
	bats[0] = bats[1] = 0;
	setibats(bats);
	getdbats(bats);
	dprintf("dbats:\n");
	for(i = 0; i < 4; i++)
		dprintf("0x%x 0x%x\n", bats[i*2], bats[i*2+1]);
	bats[0] = bats[1] = 0;
	setdbats(bats);
#endif
	return 0;
}

int arch_vm_init_endvm(kernel_args *ka)
{
	return 0;
}

void arch_vm_aspace_swap(vm_address_space *aspace)
{
}
