/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VM_PRIV_H
#define _VM_PRIV_H

#include <kernel/vm.h>

// Should only be used by vm internals
int vm_page_fault(addr address, addr fault_address, bool is_write, bool is_user);

// allocates memory from the ka structure
addr vm_alloc_from_ka_struct(kernel_args *ka, unsigned int size, int lock);

#endif

