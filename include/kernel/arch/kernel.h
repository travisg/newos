/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_KERNEL_H
#define _ARCH_KERNEL_H

#ifdef ARCH_i386
#include <kernel/arch/i386/kernel.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/kernel.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/kernel.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/kernel.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/kernel.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/kernel.h>
#endif

#define KSTACK_SIZE (PAGE_SIZE*2)
#define STACK_SIZE  (PAGE_SIZE*16)

#endif
