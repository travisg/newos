/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_ARCH_CPU_H
#define _NEWOS_ARCH_CPU_H

#ifdef ARCH_i386
#include <arch/i386/cpu.h>
#endif
#ifdef ARCH_sh4
#include <arch/sh4/cpu.h>
#endif
#ifdef ARCH_alpha
#include <arch/alpha/cpu.h>
#endif
#ifdef ARCH_sparc64
#include <arch/sparc64/cpu.h>
#endif
#ifdef ARCH_mips
#include <arch/mips/cpu.h>
#endif
#ifdef ARCH_ppc
#include <arch/ppc/cpu.h>
#endif
#ifdef ARCH_m68k
#include <arch/m68k/cpu.h>
#endif

#endif

