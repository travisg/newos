/*
** Copyright 2004 Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

pid_t 
getpgrp(void)
{
	return (pid_t)_kern_getpgid((proc_id)0);
}

