/*
** Copyright 2002, Dan Sinclair. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/*
 * This code was originally in the shell/command.c file and
 * was written by:
 *  Damien Guard
 * I just split it out into its own file
*/
#include <sys/syscalls.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* global configuration */
bool display_threads = false;

static void do_ps(void)
{
	uint32 cookie = 0;
	struct proc_info pi;
	int err;
	int count = 0;
	struct thread_info ti;
	uint32 cookie2;

	printf("process list:\n");
	printf("id\tthreads\tname\n");
	if(display_threads)
		printf("\tid\t name\t user\t kernel\n");
	for(;;) {
		err = sys_proc_get_next_proc_info(&cookie, &pi);
		if(err < 0)
			break;

		printf("%d\t%d\t%32s\n", pi.id,
				pi.num_threads, pi.name);
		count++;
		if(display_threads) {
			// display the threads this proc holds
			cookie2 = 0;
			for(;;) {
				err = sys_thread_get_next_thread_info(&cookie2, pi.id, &ti);
				if(err < 0)
					break;

				printf("\t%d\t %32s\t %Ld\t %Ld\n", ti.id, ti.name, ti.user_time, ti.kernel_time);
			}
		}
	}

	printf("\n%d processes listed\n", count);
}

static void usage(const char *progname)
{
	printf("usage:\n");
	printf("%s [-t]\n", progname);

	exit(1);
}

int main(int argc, char ** argv)
{
	char c;

	while((c = getopt(argc, argv, "t")) >= 0) {
		switch(c) {
			case 't':
				display_threads = true;
				break;
			default:
				usage(argv[0]);
		}
	}

	do_ps();

	return 0;
}


