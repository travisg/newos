/* 
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_KERNEL_H
#define _NEWOS_KERNEL_ARCH_KERNEL_H

#include INC_ARCH(kernel/arch, kernel.h)

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif
