/*
** Copyright 2002, Jeff Hamilton. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <sys/resource.h>
#include <sys/syscalls.h>

int getrlimit(int resource, struct rlimit * rlp)
{
	int retval;

	retval = sys_getrlimit(resource, rlp);

	if(retval < 0) {
		// set errno
	}

	return retval;
}

int setrlimit(int resource, const struct rlimit * rlp)
{
	int retval;

	retval = sys_setrlimit(resource, rlp);

	if(retval < 0) {
		// set errno
	}

	return retval;
}
