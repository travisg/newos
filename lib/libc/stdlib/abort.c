/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#if !_KERNEL
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscalls.h>

void abort(void)
{
	printf("abort() called\n");

	exit(0);
}
#endif

