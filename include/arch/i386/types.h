/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_TYPES_H
#define _I386_TYPES_H

#ifndef WIN32
typedef volatile unsigned long long vuint64;
typedef unsigned long long           uint64;
typedef volatile long long           vint64;
typedef long long                     int64;
#else
typedef volatile unsigned __int64   vuint64;
typedef unsigned __int64             uint64;
typedef volatile __int64             vint64;
#endif
typedef volatile unsigned int       vuint32;
typedef unsigned int                 uint32;
typedef volatile int                 vint32;
typedef int                           int32;
typedef volatile unsigned short     vuint16;
typedef unsigned short               uint16;
typedef volatile short               vint16;
typedef short                         int16;
typedef volatile unsigned char       vuint8;
typedef unsigned char                 uint8;
typedef volatile char                 vint8;
typedef char                           int8;

#endif

