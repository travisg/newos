/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _SH4_KERNEL_H
#define _SH4_KERNEL_H

#include <arch/sh4/sh4.h>

// memory layout
#define KERNEL_BASE P3_AREA
#define KERNEL_SIZE P3_AREA_LEN

#define USER_BASE   U0_AREA
#define USER_SIZE   U0_AREA_LEN

#define USER_STACK_REGION_SIZE 0x10000000
#define USER_STACK_REGION (USER_BASE + USER_SIZE - USER_STACK_REGION_SIZE)

#endif
