/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_TYPES_H
#define _I386_TYPES_H
 
#ifndef WIN32
typedef unsigned long long uint64;
typedef long long           int64;
#else
typedef unsigned __int64   uint64;
typedef __int64             int64;
#endif
typedef unsigned int       uint32;
typedef int                 int32;
typedef unsigned short     uint16;
typedef short               int16;
typedef unsigned char      uint8;
typedef char                int8;

#endif

