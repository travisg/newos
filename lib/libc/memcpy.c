/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

#include <libc/arch/string.h>

#if !_ASM_MEMCPY
#error foo

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest;
	char *s = (char *)src;

	while(count--)
		*tmp++ = *s++;

	return dest;
}

#endif
