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
	struct file_stat stat;

	rc = sys_rstat(args, &stat);
	if(rc >= 0) {
		printf("stat of file '%s': \n", args);
		printf("vnid 0x%x 0x%x\n", stat.vnid);
		printf("type %d\n", stat.type);
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
	struct file_stat stat;

	rc = sys_rstat(args, &stat);
	if(rc < 0)
		goto done_ls;

	switch(stat.type) {
		case STREAM_TYPE_FILE:
		case STREAM_TYPE_DEVICE:
			printf("%s    %d bytes\n", args, stat.size);
			count++;
			break;
		case STREAM_TYPE_DIR: {
			int fd;
			char buf[1024];
			int len;

			fd = sys_open(args, 0);
			if(fd < 0)
				goto done_ls;

			for(;;) {
				rc = sys_read(fd, buf, -1, sizeof(buf));
				if(rc <= 0)
					break;
				printf("%s\n", buf);
				count++;
			}
			sys_close(fd);
			break;
		}
		default:
			printf("%s    unknown file type\n", args);
			break;
	}

done_ls:
	printf("%d files found\n", count);
	return 0;
}

