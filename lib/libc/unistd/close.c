/*
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

int
close(int fd)
{
	int retval;

	retval= _kern_close(fd);

	if(retval< 0) {
		errno = retval;
	}

	return retval;
}
