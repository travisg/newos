/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _PPC_STAGE2_H
#define _PPC_STAGE2_H

#include <boot/stage2_struct.h>

// kernel args
typedef struct {
    // architecture specific
    addr_range page_table; // maps where the page table is located, in physical memory
    addr_range page_table_virt; // maps where the page table is mapped in virtual memory
    unsigned int page_table_mask;

    addr_range exception_handlers; // maps where the exception handler pages are mapped
} arch_kernel_args;

#endif

