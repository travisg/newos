#include <libsys/stdio.h>

int fib(int);
int shared_hello(void);

int
fib(int n)
{
	return (n<2)?1:fib(n-1)+fib(n-2);
}

int
shared_hello(void)
{
	printf("hello from a dynamic shared world\n");

	return 0;
}
