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

int main(int argc, char ** argv) {
	char *proc_state[3] = { 
		"normal","birth","death" 
		};
	const int maxprocess = 1024;
	struct proc_info *pi = (struct proc_info *) malloc(
				sizeof(struct proc_info) * maxprocess);
	int pidx;
	int procs = sys_proc_get_table(pi, sizeof(struct proc_info) * maxprocess);

	if (procs > 0) {
		printf("process list\n\n");
		printf("id\tstate\tthreads\tname\n");
		for (pidx=0; pidx<procs; pidx++) {
			printf("%d\t%s\t%d\t%s\n", pi->id,proc_state[pi->state],
					pi->num_threads, pi->name);
			pi = pi + sizeof(struct proc_info);
		}
		printf("\n%d processes listed\n", procs);
	} else {
		printf("ps: sys_get_proc_table() returned error %s!\n",strerror(procs));
	}
	return 0;
}


