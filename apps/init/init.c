/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <libsys/stdio.h>

static void setup_io()
{
	int i;

	for(i= 0; i< 256; i++) {
		sys_close(i);
	}

	sys_open("/dev/console", STREAM_TYPE_DEVICE, 0); /* stdin  */
	sys_open("/dev/console", STREAM_TYPE_DEVICE, 0); /* stdout */
	sys_open("/dev/console", STREAM_TYPE_DEVICE, 0); /* stderr */
}

int main()
{
	setup_io();

	printf("init: Welcome to NewOS!\n");

	while(1) {
		proc_id pid;

		pid = sys_proc_create_proc("/boot/bin/shell", "/boot/bin/shell", NULL, 0, 5);
		if(pid >= 0) {
			int retcode;
			printf("init: spawned shell, pid 0x%x\n", pid);
			sys_proc_wait_on_proc(pid, &retcode);
			printf("init: shell exited with return code %d\n", retcode);
		}
	}

	printf("init exiting\n");

	return 0;
}


