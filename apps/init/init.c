/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <stdio.h>
#include <sys/syscalls.h>

static void setup_io()
{
	int i;

	for(i= 0; i< 256; i++) {
		close(i);
	}

	open("/dev/console", 0); /* stdin  */
	open("/dev/console", 0); /* stdout */
	open("/dev/console", 0); /* stderr */
}

int main()
{
	setup_io();

	printf("init: Welcome to NewOS!\r\n");

	if(1) {
		proc_id pid;

		pid = sys_proc_create_proc("/boot/bin/fortune", "/boot/bin/fortune", NULL, 0, 5);
		if(pid >= 0) {
			int retcode;
			sys_proc_wait_on_proc(pid, &retcode);
		}
	}

		
	while(1) {
		proc_id pid;

		pid = sys_proc_create_proc("/boot/bin/consoled", "/boot/bin/consoled", NULL, 0, 5);
		if(pid >= 0) {
			int retcode;
			printf("init: spawned consoled, pid 0x%x\r\n", pid);
			sys_proc_wait_on_proc(pid, &retcode);
		}
	}

	printf("init exiting\r\n");

	return 0;
}


