/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

#include "commands.h"

int cmd_exit(char *args)
{
	return -1;
}

int cmd_exec(char *args)
{
	proc_id pid;

	printf("executing binary '%s'\n", args);
	
	pid = sys_proc_create_proc(args, args, 5);
	if(pid >= 0) {
		int retcode;
		sys_proc_wait_on_proc(pid, &retcode);
		printf("spawned process was pid 0x%x, retcode 0x%x\n", pid, retcode);
	}

	return 0;
}
