/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_KTYPES_H
#define _ARCH_KTYPES_H

#ifdef ARCH_i386
#include <kernel/arch/i386/ktypes.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/ktypes.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/ktypes.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/ktypes.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/ktypes.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/ktypes.h>
#endif

#endif

