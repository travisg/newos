/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libsys/syscalls.h>
#include <libsys/stdio.h>
#include <libc/string.h>
#include <libc/ctype.h>

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

int cmd_mount(char* args)
{
	char devicepath[256];
	int i, rc;

	// Copy our first argument to 'devicepath'
	for (i=0; i < 255 && *args && !isspace(*args); i++, args++) {
		devicepath[i] = *args;
	}
	devicepath[i] = '\0';

		// Skip spaces between arguments
	while(*args && isspace(*args)) {
		args++;
	}

	rc = sys_mount(devicepath, args);
	if (rc < 0) {
		printf("sys_mount() returned error %d\n", rc);
	} else {
		printf("%s successfully mounted on %s.\n", args, devicepath);
	}

	return 0;
}

int cmd_unmount(char* args)
{
	int rc;

	rc = sys_unmount(args);
	if (rc < 0) {
		printf("sys_unmount() returned error %d\n", rc);
	} else {
		printf("%s successfully unmounted.\n", args);
	}

	return 0;
}

int cmd_mkdir(char* args)
{
	int rc;

	rc = sys_create(args, STREAM_TYPE_DIR);
	if (rc < 0) {
		printf("sys_mkdir() returned error %d\n", rc);
	} else {
		printf("%s successfully created.\n", args);
	}

	return 0;
}

int cmd_cat(char* args)
{
	int rc;
	int fd;
	char buf[257];
	int len;

	fd = sys_open(args, STREAM_TYPE_FILE, 0);
	if(fd < 0) {
		printf("cat: sys_open() returned %d!\n", fd);
		goto done_cat;
	}

	for(;;) {
		rc = sys_read(fd, buf, -1, sizeof(buf) -1);
		if(rc <= 0)
			break;

		buf[rc] = '\0';
		printf("%s", buf);
	}
	sys_close(fd);

done_cat:
	return 0;
}

int cmd_cd(char *args)
{
	int rc;

	rc = sys_setcwd(args);
	if (rc < 0) {
		printf("cd: sys_setcwd() returned error %d!\n", rc);
	}

	return 0;
}

int cmd_pwd(char *args)
{
	char buf[257];
	int rc;

	rc = sys_getcwd(buf,256);
	if (rc < 0) {
		printf("cd: sys_getcwd() returned error %d!\n", rc);
	}
	buf[256] = 0;

	printf("pwd: cwd=\'%s\'\n", buf);

	return 0;
}

int cmd_stat(char *args)
{
	int rc;
	struct file_stat stat;

	rc = sys_rstat(args, &stat);
	if(rc >= 0) {
		printf("stat of file '%s': \n", args);
		printf("vnid 0x%x\n", (unsigned int)stat.vnid);
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
	if(rc < 0) {
		printf("sys_rstat() returned error %d!\n", rc);
		goto done_ls;
	}

	if(!strcmp(args, "")) {
		args = ".";
	}

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

			fd = sys_open(args, STREAM_TYPE_DIR, 0);
			if(fd < 0) {
				printf("ls: sys_open() returned %d!\n", fd);
				goto done_ls;
			}

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

int cmd_help(char *args)
{
	printf("command list:\n\n");
	printf("exit : quits this copy of the shell\n");
	printf("exec <file> : load this file as a binary and run it\n");
	printf("mkdir <path> : makes a directory at <path>\n");
	printf("cd <path> : sets the current working directory at <path>\n");
	printf("ls <path> : directory list of <path>. If no path given it lists the current dir\n");
	printf("stat <file> : gives detailed file statistics of <file>\n");
	printf("help : this command\n");
	printf("cat <file> : dumps the file to stdout\n");

	return 0;
}


