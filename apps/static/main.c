/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdio.h>
#include <unistd.h>

void setup_io()
{
	int i;

	for(i= 0; i< 256; i++) {
		close(i);
	}

	open("/dev/console", 0); /* stdin  */
	open("/dev/console", 0); /* stdout */
	open("/dev/console", 0); /* stderr */
}

int main()
{
	int i;

	setup_io();

	for(i=0; i<10; i++) {
		printf("static app\n");
	}

	return 0;
}

