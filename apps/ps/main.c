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

	if(display_threads)
		printf("   tid");
	printf("   pid  ppid  pgid   sid    user  kernel                            name\n");
	for(;;) {
		bigtime_t total_user_time = 0;
		bigtime_t total_kernel_time = 0;

		err = _kern_proc_get_next_proc_info(&cookie, &pi);
		if(err < 0)
			break;

		// display the threads this proc holds
		cookie2 = 0;
		for(;;) {
			err = _kern_thread_get_next_thread_info(&cookie2, pi.pid, &ti);
			if(err < 0)
				break;

			if(display_threads) {
				printf("%6d%6d%6d%6d%6d%8Ld%8Ld%32s\n", 
					ti.id, pi.pid, pi.ppid, pi.pgid, pi.sid, 
					ti.user_time/1000, ti.kernel_time/1000, ti.name);
			}
			total_user_time += ti.user_time;
			total_kernel_time += ti.kernel_time;
		}

		if(display_threads)
			printf("    --");
		
		printf("%6d%6d%6d%6d%8Ld%8Ld%32s\n", 
			pi.pid, pi.ppid, pi.pgid, pi.sid, 
			total_user_time/1000, total_kernel_time/1000, pi.name);
		count++;
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


