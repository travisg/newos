/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <stdio.h>

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
