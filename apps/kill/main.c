/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

void usage(int argc, char **argv)
{
	printf("invalid args, usage: %s <process id>\n", argv[1]);	

	exit(-1);
}

int main(int argc, char **argv)
{
	proc_id pid;

	if(argc < 2) {
		usage(argc, argv);
	}

	pid = atoi(argv[1]);

	printf("sending TERM signal to process %d\n", pid);

	kill(pid, SIGTERM);

	return 0;
}

