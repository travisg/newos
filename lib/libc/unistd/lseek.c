/*
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

off_t
lseek(int fd, off_t pos, int whence)
{
	off_t retval;

	retval = _kern_seek(fd, pos, whence);
	if(retval < 0)
		errno = retval;

	return retval;
}
