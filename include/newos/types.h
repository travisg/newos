/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _TYPES_H
#define _TYPES_H

#define INC_ARCH(path, x) <path/__ARCH__/x>

#include INC_ARCH(arch,types.h)

#ifndef __cplusplus

#define false 0
#define true 1
typedef int bool;

#endif

/*
   XXX serious hack that doesn't really solve the problem.
   As of right now, some versions of the toolchain expect size_t to
   be unsigned long (newer ones than 2.95.2 and beos), and the older
   ones need it to be unsigned int. It's an actual failure when
   operator new is declared. This will have to be resolved in the
   future.
*/

#ifdef ARCH_m68k
#define __SIZE_T_LONG       1
#endif

#ifdef ARCH_ppc
#define __SIZE_T_INT        1
#endif

#ifdef ARCH_sh4
#define __SIZE_T_INT        1
#endif

#if !__SIZE_T_LONG && !__SIZE_T_INT
/* when in doubt, set it to LONG */
#define __SIZE_T_LONG       1
#endif

/* uncomment the following if you want to override LONG vs INT */
#if 0
#ifdef __SIZE_T_LONG
#undef __SIZE_T_LONG
#endif
#ifdef __SIZE_T_INT
#undef __SIZE_T_INT
#endif

/* to override the LONG vs INT setting, set *one* of the below to 1 */
#define __SIZE_T_LONG       0
#define __SIZE_T_INT        1
#endif

// note that we only declare a proxy typedef here for size_t, called _newos_size_t
// which stddef.h then retypedefs as the actual size_t. ssize_t is not C++ standard,
// so it belongs here as well as anywhere.

#if __SIZE_T_LONG
typedef unsigned long       _newos_size_t;
typedef signed long         ssize_t;
#elif __SIZE_T_INT
typedef unsigned int        _newos_size_t;
typedef signed int          ssize_t;
#else
#error "Don't know what size_t should be (int or long)!"
#endif

typedef int64               off_t;

typedef unsigned char       u_char;
typedef unsigned short      u_short;
typedef unsigned int        u_int;
typedef unsigned long       u_long;

typedef unsigned char       uchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;

/* system types */
typedef int64 bigtime_t;
typedef uint64 vnode_id;
typedef int region_id;      // vm region id
typedef int aspace_id;      // address space id
typedef int thread_id;      // thread id
typedef int proc_id;        // process id
typedef int pgrp_id;        // process group id
typedef int sess_id;        // session id
typedef int sem_id;         // semaphore id
typedef int port_id;        // ipc port id
typedef int image_id;       // binary image id

# include <stddef.h>

#endif

