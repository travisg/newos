/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef __NEWOS_COMPILER_H
#define __NEWOS_COMPILER_H

#ifndef __ASSEMBLY__

#if __GNUC__ == 3
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define _PACKED __attribute__((packed))

#endif // #ifndef __ASSEMBLY__

#endif

