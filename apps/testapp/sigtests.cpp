/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>

static void alarm_handler(int sig, void *arg)
{
    char temp[128];
    sprintf(temp, "alarm %d %d\n", sig, arg);
    write(0, temp, strlen(temp));
}

static void term_handler(int sig, void *arg)
{
    char temp[128];
    sprintf(temp, "TERM %d %d\n", sig, arg);
    write(0, temp, strlen(temp));
}

static void chld_handler(int sig, void *arg)
{
    char temp[128];
    sprintf(temp,"SIGCHLD %d\n", sig);
    write(0, temp, strlen(temp));
}

static void winch_handler(int sig, void *arg)
{
    char temp[128];
    sprintf(temp, "SIGWINCH %d\n", sig);
    write(0, temp, strlen(temp));
}

int sig_test(int arg)
{
    struct sigaction sig;
    int err;

    sig.sa_handler = (sig_func_t)&alarm_handler;
    sig.sa_mask = 0;
    sig.sa_flags = 0;
    sig.sa_userdata = (void *)1170;

    err = _kern_sigaction(SIGALRM, &sig, NULL);
    printf("sigaction returns %d\n", err);

    sig.sa_handler = (sig_func_t)&term_handler;
    sig.sa_mask = 0;
    sig.sa_flags = 0;
    sig.sa_userdata = (void *)1170;

    err = _kern_sigaction(SIGTERM, &sig, NULL);
    printf("sigaction returns %d\n", err);

    sig.sa_handler = (sig_func_t)&chld_handler;
    sig.sa_mask = 0;
    sig.sa_flags = 0;
    sig.sa_userdata = (void *)1170;

    err = _kern_sigaction(SIGCHLD, &sig, NULL);
    printf("sigaction returns %d\n", err);

    sig.sa_handler = (sig_func_t)&winch_handler;
    sig.sa_mask = 0;
    sig.sa_flags = 0;
    sig.sa_userdata = (void *)1170;

    err = _kern_sigaction(SIGWINCH, &sig, NULL);
    printf("sigaction returns %d\n", err);

    _kern_set_alarm(1000000, TIMER_MODE_PERIODIC);

    for (;;) {
        err = _kern_snooze(10000000);
        printf("_kern_snooze returns %d\n", err);
        _kern_proc_create_proc("/boot/bin/false", "false", NULL, 0, 5, 0);
    }
    return 0;
}


