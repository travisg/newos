#ifndef _TYPES_H
#define _TYPES_H

#ifdef ARCH_i386
#include <i386/types.h>
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
#define false 0
#define true 1
typedef int bool;
*/

typedef unsigned int	size_t;
typedef int				ssize_t;

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned int		u_int;
typedef unsigned long		u_long;

typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#endif

