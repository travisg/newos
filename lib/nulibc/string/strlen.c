/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

size_t
strlen(char const *s)
{
	size_t i;

	i= 0;
	while(s[i]) {
		i+= 1;
	}

	return i;
}
