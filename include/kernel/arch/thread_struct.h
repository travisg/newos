/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_THREAD_STRUCT_H
#define _ARCH_THREAD_STRUCT_H

#ifdef ARCH_i386
#include <kernel/arch/i386/thread_struct.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/thread_struct.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/thread_struct.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/thread_struct.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/thread_struct.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/thread_struct.h>
#endif

#endif

