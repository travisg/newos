/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

void *
bcopy(void const *src, void *dest, size_t count)
{
	return memcpy(dest, src, count);
}

