/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_KERNEL_H
#define _I386_KERNEL_H

#include <kernel/arch/i386/cpu.h>

// memory layout
#define KERNEL_BASE 0x80000000
#define KERNEL_SIZE 0x80000000

#define USER_BASE   0
#define USER_SIZE   0x80000000

#define USER_STACK_REGION 0x70000000
#define USER_STACK_REGION_SIZE 0x10000000

#endif
