/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "commands.h"
#include "file_utils.h"
#include "shell_defs.h"

struct command cmds[] = {
	{"exec", &cmd_exec},
	{"ls", &cmd_ls},
	{"stat", &cmd_stat},
	{"mount", &cmd_mount},
	{"unmount", &cmd_unmount},
	{"mkdir", &cmd_mkdir},
	{"cat", &cmd_cat},
	{"cd", &cmd_cd},
	{"pwd", &cmd_pwd},
	{"help", &cmd_help},
	{NULL, NULL}
};


int cmd_exec(int argc, char *argv[])
{
	return cmd_create_proc(argc - 1,argv+1);
}

int cmd_create_proc(int argc,char *argv[])
{
	bool must_wait=true;
	proc_id pid;

	int  arg_len;
	char *tmp;
	char filename[SCAN_SIZE+1];

	if(argc  <1){
		printf("not enough args to exec\n");
		return 0;
	}

	tmp =  argv[argc - 1];

	if( !find_file_in_path(argv[0],filename,SCAN_SIZE)){
		printf("can't find '%s' \n",argv[0]);
		return 0;
	}


	// a hack to support the unix '&'
	if(argc >= 1) {
		arg_len = strlen(tmp);
		if(arg_len > 0){
			tmp += arg_len -1;
			if(*tmp == '&'){
				if(arg_len == 1){
					argc --;
				} else {
					*tmp = 0;
				}
				must_wait = false;
			}
		}
	}

	pid = sys_proc_create_proc(filename,filename, argv, argc, 5);
	if(pid >= 0) {
		int retcode;

		if(must_wait) {
			sys_proc_wait_on_proc(pid, &retcode);
		}
	} else {
		printf("Error: cannot execute '%s'\n", filename);
		return 0; // should be -1, but the shell would exit
	}

	return 0;
}

int cmd_mount(int argc, char *argv[])
{
	int i, rc;

	if(argc < 4) {
		printf("not enough arguments to mount:\n");
		printf("usage: mount <path> <device> <fsname>\n");
		return 0;
	}

	rc = sys_mount(argv[1], argv[2], argv[3], NULL);
	if (rc < 0) {
		printf("sys_mount() returned error: %s\n", strerror(rc));
	} else {
		printf("%s successfully mounted on %s.\n", argv[2], argv[1]);
	}

	return 0;
}

int cmd_unmount(int argc, char *argv[])
{
	int rc;

	if(argc < 2) {
		printf("not enough arguments to unmount\n");
		return 0;
	}

	rc = sys_unmount(argv[1]);
	if (rc < 0) {
		printf("sys_unmount() returned error: %s\n", strerror(rc));
	} else {
		printf("%s successfully unmounted.\n", argv[1]);
	}

	return 0;
}

int cmd_mkdir(int argc, char *argv[])
{
	int rc;

	if(argc < 2) {
		printf("not enough arguments to mkdir\n");
		return 0;
	}

	rc = sys_create(argv[1], STREAM_TYPE_DIR);
	if (rc < 0) {
		printf("sys_mkdir() returned error: %s\n", strerror(rc));
	} else {
		printf("%s successfully created.\n", argv[1]);
	}

	return 0;
}

int cmd_cat(int argc, char *argv[])
{
	int rc;
	int fd;
	char buf[257];
	int len;

	if(argc < 2) {
		printf("not enough arguments to cat\n");
		return 0;
	}

	fd = sys_open(argv[1], STREAM_TYPE_FILE, 0);
	if(fd < 0) {
		printf("cat: sys_open() returned error: %s!\n", strerror(fd));
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

int cmd_cd(int argc, char *argv[])
{
	int rc;

	if(argc < 2) {
		printf("not enough arguments to cd\n");
		return 0;
	}

	rc = sys_setcwd(argv[1]);
	if (rc < 0) {
		printf("cd: sys_setcwd() returned error: %s!\n", strerror(rc));
	}

	return 0;
}

int cmd_pwd(int argc, char *argv[])
{
	char buf[257];
	int rc;

	rc = sys_getcwd(buf,256);
	if (rc < 0) {
		printf("cd: sys_getcwd() returned error: %s!\n", strerror(rc));
	}
	buf[256] = 0;

	printf("pwd: cwd=\'%s\'\n", buf);

	return 0;
}

int cmd_stat(int argc, char *argv[])
{
	int rc;
	struct file_stat stat;

	if(argc < 2) {
		printf("not enough arguments to stat\n");
		return 0;
	}

	rc = sys_rstat(argv[1], &stat);
	if(rc >= 0) {
		printf("stat of file '%s': \n", argv[1]);
		printf("vnid 0x%x\n", (unsigned int)stat.vnid);
		printf("type %d\n", stat.type);
		printf("size %d\n", (int)stat.size);
	} else {
		printf("stat failed for file '%s'\n", argv[1]);
	}
	return 0;
}

int cmd_ls(int argc, char *argv[])
{
	int rc;
	int count = 0;
	struct file_stat stat;
	char *arg;

	if(argc == 1) {
		arg = ".";
	} else {
		arg = argv[1];
	}

	rc = sys_rstat(arg, &stat);
	if(rc < 0) {
		printf("sys_rstat() returned error: %s!\n", strerror(rc));
		goto err_ls;
	}

	switch(stat.type) {
		case STREAM_TYPE_FILE:
		case STREAM_TYPE_DEVICE:
			printf("%s    %d bytes\n", arg, (int)stat.size);
			count++;
			break;
		case STREAM_TYPE_DIR: {
			int fd;
			char buf[1024];
			int len;

			fd = sys_open(arg, STREAM_TYPE_DIR, 0);
			if(fd < 0) {
				printf("ls: sys_open() returned error: %s!\n", strerror(fd));
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
			printf("%s    unknown file type\n", arg);
			break;
	}

done_ls:
	printf("%d files found\n", count);
err_ls:
	return 0;
}

int cmd_help(int argc, char *argv[])
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
	printf("mount <path> <device> <fsname> : tries to mount <device> at <path>\n");
	printf("unmount <path> : tries to unmount at <path>\n");

	return 0;
}


