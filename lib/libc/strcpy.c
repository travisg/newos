/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

char *strcpy(char *dest, const char *src)
{
	char *tmp = dest;

	while((*dest++ = *src++) != '\0')
		;
	return tmp;
}

