/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _TYPES_H
#define _TYPES_H

#ifdef ARCH_i386
#include <arch/i386/types.h>
#endif
#ifdef ARCH_alpha
#include <arch/alpha/types.h>
#endif
#ifdef ARCH_sh4
#include <arch/sh4/types.h>
#endif
#ifdef ARCH_sparc
#include <arch/sparc/types.h>
#endif
#ifdef ARCH_sparc64
#include <arch/sparc64/types.h>
#endif
#ifdef ARCH_mips
#include <arch/mips/types.h>
#endif
#ifdef ARCH_ppc
#include <arch/ppc/types.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#define false 0
#define true 1
typedef int bool;

typedef uint32              size_t;
typedef int32               ssize_t;
typedef int64               off_t;

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned int		u_int;
typedef unsigned long		u_long;

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#endif

