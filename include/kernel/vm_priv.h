/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VM_PRIV_H
#define _VM_PRIV_H

#include <kernel/vm.h>

// Should only be used by vm internals
int vm_get_free_page_run(unsigned int *page, unsigned int len);
int vm_get_free_page(unsigned int *page);
int vm_page_fault(int address, unsigned int fault_address);
int vm_mark_page_inuse(unsigned int page);
int vm_mark_page_range_inuse(unsigned int start_page, unsigned int len);

#endif

