/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef KERNEL
#include <stdlib.h>
#include <sys/syscalls.h>

void abort(void)
{
	proc_id id = sys_get_current_proc_id();

	sys_proc_kill_proc(id);
}
#endif

