/* 
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <sys/syscalls.h>


ssize_t
write(int fd, void const *buf, size_t len)
{
	int retval;

	retval= sys_write(fd, buf, -1, len);

	if(retval< 0) {
		// set errno
	}

	return retval;
}
