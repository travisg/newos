/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_STAGE2_H
#define _ARCH_STAGE2_H

#ifdef ARCH_i386
#include <boot/arch/i386/stage2.h>
#endif
#ifdef ARCH_sh4
#include <boot/arch/sh4/stage2.h>
#endif
#ifdef ARCH_sparc64
#include <boot/arch/sparc64/stage2.h>
#endif
#ifdef ARCH_mips
#include <boot/arch/mips/stage2.h>
#endif
#ifdef ARCH_ppc
#include <boot/arch/ppc/stage2.h>
#endif

#endif
