/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>

#ifdef ARCH_i386
#include <bus/pci/pci_bus.h>
#endif
#ifdef ARCH_sh4
#endif

int bus_init(kernel_args *ka)
{

#ifdef ARCH_i386
	pci_bus_init(ka);
#endif	

#ifdef ARCH_sh4
#endif

	return 0;
}
