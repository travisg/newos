/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#if !_KERNEL
#include <stdlib.h>
#include <sys/syscalls.h>

void abort(void)
{
	proc_id id = _kern_get_current_proc_id();

	_kern_proc_kill_proc(id);
}
#endif

