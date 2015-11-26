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

    printf("\nloading girlfriend.so ...\n");
    freston= dlopen("/boot/lib/girlfriend.so", RTLD_NOW);
    girlfriend= dlsym(freston, "girlfriend");
    ((void(*)(void))girlfriend)();
    dlclose(freston);
    printf("unloaded girlfriend.so ...\n");


    printf("\nloading girlfriend.so ...\n");
    freston= dlopen("/boot/lib/girlfriend.so", RTLD_NOW);
    freston= dlopen("/boot/lib/girlfriend.so", RTLD_NOW);
    girlfriend= dlsym(freston, "girlfriend");
    ((void(*)(void))girlfriend)();
    dlclose(freston);
    ((void(*)(void))girlfriend)();
    dlclose(freston);
    printf("unloaded girlfriend.so ...\n");

    printf("\ncalling girlfriend again (must page fault)...\n");
    ((void(*)(void))girlfriend)();
    printf("If you are reading this... something went really wrong\n");

    return 0;
}
