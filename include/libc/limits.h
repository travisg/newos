/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _NEWOS_LIBC_LIMITS_H
#define _NEWOS_LIBC_LIMITS_H

#include <newos/types.h>

#define INT8_MAX   0x7f
#define INT8_MIN   (-0x7f - 1)
#define UINT8_MAX  0xff
#define UINT8_MIN  0

#define INT16_MAX   0x7fff
#define INT16_MIN   (-0x7fff - 1)
#define UINT16_MAX  0xffff
#define UINT16_MIN  0

#define INT32_MAX   0x7fffffff
#define INT32_MIN   (-0x7ffffffff - 1)
#define UINT32_MAX  0xffffffffU
#define UINT32_MIN  0

#define INT64_MAX   0x7fffffffffffffffLL
#define INT64_MIN   (-0x7fffffffffffffffLL - 1)
#define UINT64_MAX  0xffffffffffffffffULL
#define UINT64_MIN  0

#include INC_ARCH(libc/arch, limits.h)

#endif
