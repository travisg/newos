/*
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

int
open(char const *path, int omode, ...)
{
	int retval;

	retval= _kern_open(path, STREAM_TYPE_ANY , omode);

	if(retval< 0) {
		errno = retval;
	}

	return retval;
}
