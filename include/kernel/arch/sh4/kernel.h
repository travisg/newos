#ifndef _ARCH_KERNEL_H
#define _ARCH_KERNEL_H

#include <sh4.h>

// memory layout
#define KERNEL_BASE P3_AREA
#define KERNEL_SIZE P3_AREA_LEN

#define USER_BASE   U0_AREA
#define USER_SIZE   U0_AREA_LEN

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif
