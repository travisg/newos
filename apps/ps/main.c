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

#define DISPLAY_THREADS 0

int main(int argc, char ** argv)
{
	uint32 cookie = 0;
	struct proc_info pi;
	int err;
	int count = 0;
#if DISPLAY_THREADS
	struct thread_info ti;
	uint32 cookie2;
#endif

	printf("process list:\n");
	printf("id\tthreads\tname\n");
#if DISPLAY_THREADS
	printf(" id\t name\t user\t kernel\n");
#endif
	for(;;) {
		err = sys_proc_get_next_proc_info(&cookie, &pi);
		if(err < 0)
			break;

		printf("%d\t%d\t%s\n", pi.id,
				pi.num_threads, pi.name);
		count++;
#if DISPLAY_THREADS
		// display the threads this proc holds
		cookie2 = 0;
		for(;;) {
			err = sys_thread_get_next_thread_info(&cookie2, pi.id, &ti);
			if(err < 0)
				break;

			printf(" %d\t %s\t %Ld\t %Ld\n", ti.id, ti.name, ti.user_time, ti.kernel_time);
		}
#endif
	}

	printf("\n%d processes listed\n", count);

	return 0;
}


