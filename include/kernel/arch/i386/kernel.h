#ifndef _ARCH_KERNEL_H
#define _ARCH_KERNEL_H

#define PAGE_SIZE 4096

// memory layout
#define KERNEL_BASE 0x80000000
#define KERNEL_SIZE 0x80000000

#define USER_BASE   0
#define USER_SIZE   0x80000000

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif
