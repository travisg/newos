/*
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

int
ioctl(int fd, int op, void *buf, size_t len)
{
	int retval;

	retval= _kern_ioctl(fd, op, buf, len);

	if(retval< 0) {
		errno = retval;
	}

	return retval;
}
