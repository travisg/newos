/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_DEBUG
#define _NEWOS_KERNEL_ARCH_DEBUG

int arch_dbg_init(kernel_args *ka);

#ifdef ARCH_i386
#include <kernel/arch/i386/debug.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/debug.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/debug.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/debug.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/debug.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/debug.h>
#endif
#ifdef ARCH_m68k
#include <kernel/arch/m68k/debug.h>
#endif

#endif

