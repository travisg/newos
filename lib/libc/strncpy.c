/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

char *strncpy(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	while(count-- && (*dest++ = *src++) != '\0')
		;

	return tmp;
}

