/*
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

ssize_t
read(int fd, void *buf, size_t len)
{
	int retval;

	retval= _kern_read(fd, buf, -1, len);

	if(retval< 0) {
		errno = retval;
	}

	return retval;
}
