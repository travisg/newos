/* 
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <sys/syscalls.h>


int
dup(int fd)
{
	int retval;

	retval= _kern_dup(fd);

	if(retval< 0) {
		// set errno
	}

	return retval;
}
