/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <stdio.h>
#include <dlfcn.h>

extern int fib(int);
extern void shared_hello(void);

int
main()
{
	void *freston;
	void *girlfriend;

	shared_hello();
	printf("%d %d %d %d %d %d\n", fib(0), fib(1), fib(2), fib(3), fib(4), fib(5));

	freston= dlopen("/boot/lib/girlfriend.so", RTLD_NOW);
	girlfriend= dlsym(freston, "girlfriend");

	((void(*)(void))girlfriend)();

	return 0;
}
