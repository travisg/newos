#ifndef _KERNEL_H
#define _KERNEL_H

#include <ktypes.h>

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

#define KSTACK_SIZE (PAGE_SIZE*2)

#define min(a, b) ((a) > (b) ? (a) : (b))

#define CHECK_BIT(a, b) ((a) & (1 << (b)))
#define SET_BIT(a, b) ((a) | (1 << (b)))
#define CLEAR_BIT(a, b) ((a) & (~(1 << (b))))

#define TOUCH(a) ((a) = (a))

#endif

