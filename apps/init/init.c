/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

int main()
{
	printf("init: Welcome to NewOS!\n");

	while(1) {
		proc_id pid;

		pid = sys_proc_create_proc("/boot/bin/shell", "/boot/bin/shell", 5);
		if(pid >= 0) {
			int retcode;
			printf("init: spawned shell, pid 0x%x\n", pid);
			sys_proc_wait_on_proc(pid, &retcode);
		}
	}

	printf("init exiting\n");

	return 0;
}


