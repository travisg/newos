/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <sys/syscalls.h>
#include <newos/pipefs_priv.h>

int
pipe(int fds[2])
{
	int fd;
	int err;

	fd = open("/pipe/anon", 0);
	if(fd < 0)
		return fd;

	err = sys_ioctl(fd, _PIPEFS_IOCTL_CREATE_ANONYMOUS, fds, sizeof(int) * 2);

	close(fd);

	return err;
}
