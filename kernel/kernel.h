#ifndef _KERNEL_H
#define _KERNEL_H

#include <ktypes.h>

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define CHECK_BIT(a, b) ((a) & (1 << (b)))
#define SET_BIT(a, b) ((a) | (1 << (b)))
#define CLEAR_BIT(a, b) ((a) & (~(1 << (b))))

#define TOUCH(a) ((a) = (a))

// memory layout
#define KERNEL_BASE 0x80000000
#define KERNEL_SIZE 0x80000000

#define USER_BASE   0x0
#define USER_SIZE   0x80000000

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif

