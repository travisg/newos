/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

size_t strlen(const char * s)
{
	const char *sc;

	for(sc = s; *sc != '\0'; ++sc)
		;
	return sc - s;
}
