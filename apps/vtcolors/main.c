/*
** Copyright 2002, Trey Boudreau. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdio.h>

int main(void)
{
	int fg, bg;
	printf("\n");
	for (bg = 40; bg < 48; bg++)
	{
		// set the bg color
		printf("\x1b[%d;1m", bg);	// color, bright
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\x1b[2m");			// dim
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\n");
	}
	for (bg = 40; bg < 48; bg++)
	{
		// set the bg color
		printf("\x1b[%d;1;5m", bg); // color, bright, blink
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\x1b[2m");			// dim
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\n");
	}
	printf("\x1b[0m");
	return 0;
}

