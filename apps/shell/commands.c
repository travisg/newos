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

int cmd_stat(char *args)
{
	int rc;
	struct vnode_stat stat;

	rc = sys_stat(args, "", STREAM_TYPE_FILE, &stat);
	if(rc < 0)
		rc = sys_stat(args, "", STREAM_TYPE_DEVICE, &stat);
	if(rc < 0)
		rc = sys_stat(args, "", STREAM_TYPE_DIR, &stat);
	if(rc >= 0) {
		printf("stat of file '%s': \n", args);
		printf("size %d\n", stat.size);
	} else {
		printf("stat failed for file '%s'\n", args);
	}
	return 0;
}

int cmd_ls(char *args)
{
	int rc;
	int count = 0;
	struct vnode_stat stat;

	rc = sys_stat(args, "", STREAM_TYPE_FILE, &stat);
	if(rc < 0)
		rc = sys_stat(args, "", STREAM_TYPE_DEVICE, &stat);
	if(rc >= 0) {
		printf("%s    %d bytes\n", args, stat.size);
		count++;
		goto done_ls;
	}

	rc = sys_stat(args, "", STREAM_TYPE_DIR, &stat);
	if(rc >= 0) {
		int fd;
		char buf[1024];
		int len;

		fd = sys_open(args, "", STREAM_TYPE_DIR);
		if(fd < 0)
			goto done_ls;

		for(;;) {
			len = sizeof(buf);
			rc = sys_read(fd, buf, -1, &len);
			if(rc < 0)
				break;
			if(len <= 0)
				break;
			printf("%s\n", buf);
			count++;
		}
		sys_close(fd);
	}
done_ls:
	printf("%d files found\n", count);
	return 0;
}

