/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_BOOT_STAGE2_STRUCT_H
#define _NEWOS_BOOT_STAGE2_STRUCT_H

#include <kernel/ktypes.h>

#define MAX_PHYS_MEM_ADDR_RANGE 16
#define MAX_VIRT_ALLOC_ADDR_RANGE 16
#define MAX_PHYS_ALLOC_ADDR_RANGE 16

typedef struct {
	addr_t start;
	addr_t size;
} addr_range;

#endif

