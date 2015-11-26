/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>

static int fpu_test_thread(void *arg)
{
    thread_id tid = _kern_get_current_thread_id();
    double foo = tid;
    double bar;

    printf("starting thread %d\n", tid);

    for (;;) {
        bar = foo * 1.47;
        foo = bar / 1.47;
        if (foo != (double)tid)
            printf("error: foo %f, should be %f\n", foo, (double)tid);
    }

    return 0;
}

int fpu_test(int arg)
{
    int i;

    for (i=0; i<5; i++) {
        thread_id tid = _kern_thread_create_thread("fpu tester", &fpu_test_thread, 0);
        _kern_thread_set_priority(tid, 1);
        _kern_thread_resume_thread(tid);
    }

    printf("finished creating threads, waiting forever\n");
    for (;;)
        usleep(1000000);

    return 0;
}

