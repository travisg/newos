/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* globals */
void (*disp_func)(const char *, struct file_stat *) = NULL;
bool full_list = false;

static void display_l(const char *filename, struct file_stat *stat)
{
	const char *type;

	switch(stat->type) {
		case STREAM_TYPE_FILE:
			type = "FILE";
			break;
		case STREAM_TYPE_DEVICE:
			type = "DEV ";
			break;
		case STREAM_TYPE_DIR:
			type = "DIR ";
			break;
		case STREAM_TYPE_PIPE:
			type = "PIPE";
			break;
		default:
			type = "UNKN";
	}

	printf("%s %12Ld %s\n", type, stat->size, filename);
}

// unused
static void display(const char *filename, struct file_stat *stat)
{
	printf("%s\n", filename);
}

static void usage(const char *progname)
{
	printf("usage:\n");
	printf("%s [-al] [file ...]\n", progname);

	exit(1);
}

static int do_ls(const char *arg)
{
	int rc;
	int rc2;
	struct file_stat stat;
	int count = 0;

	rc = _kern_rstat(arg, &stat);
	if(rc < 0) {
		printf("_kern_rstat() returned error: %s!\n", strerror(rc));
		return rc;
	}

	switch(stat.type) {
		case STREAM_TYPE_DIR: {
			int fd;
			char filename[1024];
			bool done_dot, done_dotdot;

			fd = _kern_opendir(arg);
			if(fd < 0) {
				//printf("ls: opendir() returned error: %s!\n", strerror(fd));
				break;
			}

			if(strcmp(arg, ".") != 0) {
				printf("%s:\n", arg);
			}

			if(full_list) {
				done_dot = done_dotdot = false;
			} else {
				done_dot = done_dotdot = true;
			}

			for(;;) {
				char full_path[1024];

				if(!done_dot) {
					strlcpy(filename, ".", sizeof(filename));
					done_dot = true;
				} else if(!done_dotdot) {
					strlcpy(filename, "..", sizeof(filename));
					done_dotdot = true;
				} else {
					rc = _kern_readdir(fd, filename, sizeof(filename));
					if(rc <= 0)
						break;
				}

				full_path[0] = 0;
				if(strcmp(arg, ".") != 0) {
					strlcpy(full_path, arg, sizeof(full_path));
					strlcat(full_path, "/", sizeof(full_path));
				}
				strlcat(full_path, filename, sizeof(full_path));

				rc2 = _kern_rstat(full_path, &stat);
				if(rc2 >= 0) {
					(*disp_func)(filename, &stat);
				}
				count++;
			}
			_kern_closedir(fd);

			printf("%d files found\n", count);
			break;
		}
		default:
			(*disp_func)(arg, &stat);
			break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char c;

	disp_func = &display;

	while((c = getopt(argc, argv, "al")) >= 0) {
		switch(c) {
			case 'a':
				full_list = true;
				break;
			case 'l':
				disp_func = &display_l;
				break;
			default:
				usage(argv[0]);
		}
	}

	if(optind >= argc) {
		// no arguments to ls. ls the current dir
		do_ls(".");
	} else {
		for(; optind < argc; optind++) {
			do_ls(argv[optind]);
		}
	}

	return 0;
}


