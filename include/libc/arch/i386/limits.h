/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _NEWOS_LIBC_LIMITS_I386_H
#define _NEWOS_LIBC_LIMITS_I386_H

#define SCHAR_MAX INT8_MAX
#define SCHAR_MIN INT8_MIN
#define UCHAR_MAX UINT8_MAX
#define UCHAR_MIN UINT8_MIN
#define CHAR_MAX  SCHAR_MAX
#define CHAR_MIN  SCHAR_MIN

#define USHRT_MAX UINT16_MAX
#define USHRT_MIN UINT16_MIN
#define SHRT_MAX  INT16_MAX
#define SHRT_MIN  INT16_MIN

#define UINT_MAX  UINT32_MAX
#define UINT_MIN  UINT32_MIN
#define INT_MAX   INT32_MAX
#define INT_MIN   INT32_MIN

#define ULONG_MAX UINT32_MAX
#define ULONG_MIN UINT32_MIN
#define LONG_MAX  INT32_MAX
#define LONG_MIN  INT32_MIN

#define ULLONG_MAX UINT64_MAX
#define ULLONG_MIN UINT64_MIN
#define LLONG_MAX  INT64_MAX
#define LLONG_MIN  INT64_MIN

#endif
