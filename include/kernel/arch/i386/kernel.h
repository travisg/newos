#ifndef _I386_KERNEL_H
#define _I386_KERNEL_H

#include <kernel/arch/i386/cpu.h>

// memory layout
#define KERNEL_BASE 0x80000000
#define KERNEL_SIZE 0x80000000

#define USER_BASE   0
#define USER_SIZE   0x80000000

#endif
