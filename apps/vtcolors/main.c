/*
** Copyright 2002, Trey Boudreau. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
int main(void)
{
	int fg, bg;
	printf("\n");
	for (bg = 40; bg < 48; bg++)
	{
		// set the bg color
		printf("\x1b[%d;1m", bg);
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\x1b[2m");
		for (fg = 30; fg < 38; fg++)
		{
			printf("\x1b[%dm**", fg);
		}
		printf("\n");
	}
	printf("\x1b[0m");
	return 0;
}

