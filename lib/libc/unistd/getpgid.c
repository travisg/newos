/*
** Copyright 2004 Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

pid_t 
getpgid(pid_t pid)
{
	return (pid_t)_kern_getpgid((proc_id)pid);
}

