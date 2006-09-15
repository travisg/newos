/*
** Copyright 2004-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_ARCH_I386_ENDIAN_H
#define _NEWOS_ARCH_I386_ENDIAN_H

#define BYTE_ORDER LITTLE_ENDIAN

#define _ARCH_BSWAP32(x) \
({ \
	unsigned int temp = (x); \
	__asm__ volatile ("bswap %0" : "+r" (temp)); \
	temp; \
})

#endif

