/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>

static int pipe_read_thread(void *args)
{
	int fd = *(int *)args;
	int err;
	char buf[1024];
	int i;

	for(;;) {
		err = read(fd, buf, sizeof(buf));
		printf("pipe_read_thread: read returns %d\n", err);
		if(err < 0)
			break;

		printf("'");
		for(i=0; i<err; i++)
			printf("%c", buf[i]);
		printf("'\n");
	}

	return err;
}

static int pipe_test(void)
{
	int fds[2];
	int err;
	char buf[1024];
	thread_id id;

	err = pipe(fds);
	printf("pipe returns %d\n", err);
	printf("%d %d\n", fds[0], fds[1]);

#if 1
	id = sys_thread_create_thread("pipe read thread", &pipe_read_thread, &fds[1]);
	sys_thread_resume_thread(id);

	sys_snooze(2000000);

	err = write(fds[0], "this is a test", sizeof("this is a test"));
	printf("write returns %d\n", err);
	if(err < 0)
		return err;

	sys_snooze(2000000);

	close(fds[0]);
	close(fds[1]);
#endif
#if 0
	// close the reader end and write to it
	close(fds[1]);
	err = write(fds[0], "this is a test", sizeof("this is a test"));
	printf("write returns %d\n", err);

	sys_snooze(2000000);
#endif
	return 0;
}


