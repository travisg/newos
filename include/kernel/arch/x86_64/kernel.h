/*
** Copyright 2004-2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_X86_64_KERNEL_H
#define _NEWOS_KERNEL_ARCH_X86_64_KERNEL_H

// memory layout
#define KERNEL_BASE (0xffff800000000000UL)
#define KERNEL_ALLOC_BASE (0xffffffff80000000UL)
#define KERNEL_SIZE (0x00007fffffffffffUL)
#define KERNEL_TOP  (KERNEL_BASE + (KERNEL_SIZE - 1))

#define KERNEL_ADDR_MASK (0x8000000000000000UL)

// a macro to test if a pointer is inside kernel space
#define is_kernel_address(x) (((addr_t)(x)) & KERNEL_ADDR_MASK)

/*
** User space layout is a little special:
** The user space does not completely cover the space not covered by the kernel.
** This is accomplished by starting user space at 1Mb and running to 64kb short of kernel space.
** The lower 1Mb reserved spot makes it easy to find null pointer references and guarantees a
** region wont be placed there. The 64kb region assures a user space thread cannot pass
** a buffer into the kernel as part of a syscall that would cross into kernel space.
*/
#define USER_BASE   (0x100000UL)
#define USER_TOP    (0x00007fffffffffffUL)
#define USER_SIZE   (USER_TOP - USER_BASE)

// a macro to test if a pointer is inside user space
#define is_user_address(x) (((addr_t)(x)) >= USER_BASE && ((addr_t)(x)) <= USER_TOP)

#define USER_STACK_REGION 0x70000000
#define USER_STACK_REGION_SIZE (USER_BASE + (USER_SIZE - USER_STACK_REGION))

#endif
