/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

void *
memset(void *s, int c, size_t count)
{
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
}
