/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

void
bzero(void *dst, size_t count)
{
	memset(dst, 0, count);
}

