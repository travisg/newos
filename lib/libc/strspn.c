/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

size_t strspn(const char *s, const char *accept)
{
	const char *p;
	const char *a;
	size_t count = 0;

	for(p = s; *p != '\0'; ++p) {
		for(a = accept; *a != '\0'; ++a) {
			if(*p == *a)
				break;
		}
		if(*a == '\0')
			return count;
		++count;
	}

	return count;
}
