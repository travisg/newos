/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

char *strncat(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	if(count > 0) {
		while(*dest)
			dest++;
		while((*dest++ = *src++)) {
			if (--count == 0) {
				*dest = '\0';
				break;
			}
		}
	}

	return tmp;
}

