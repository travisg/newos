#ifndef _TYPES_H
#define _TYPES_H

#ifdef ARCH_i386
#include <i386/types.h>
#endif
#ifdef ARCH_alpha
#include <alpha/types.h>
#endif
#ifdef ARCH_sh4
#include <sh4/types.h>
#endif
#ifdef ARCH_sparc
#include <sparc/types.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#define false 0
#define true 1
typedef int bool;

typedef uint32              size_t;
typedef int32               ssize_t;
typedef uint64              off_t;

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned int		u_int;
typedef unsigned long		u_long;

typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#endif

