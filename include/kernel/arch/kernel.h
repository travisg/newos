#ifndef _ARCH_KERNEL_H
#define _ARCH_KERNEL_H

#ifdef ARCH_i386
#include <kernel/arch/i386/kernel.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/kernel.h>
#endif

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif
