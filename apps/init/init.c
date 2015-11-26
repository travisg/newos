/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <stdio.h>
#include <sys/syscalls.h>

static void setup_io()
{
    int i;

    for (i= 0; i< 256; i++) {
        close(i);
    }

    open("/dev/console", 0); /* stdin  */
    open("/dev/console", 0); /* stdout */
    open("/dev/console", 0); /* stderr */
}

int main()
{
    setup_io();

    printf("init: Welcome to NewOS!\r\n");

    {
        proc_id pid;
        char *args[2];

        args[0] = "/boot/bin/shell";
        args[1] = "/boot/bootscript";

        pid = _kern_proc_create_proc(args[0], "startup shell", args, 2, 5, 0);
        if (pid < 0) {
            printf("init: error %d creating shell!\n", pid);
            return -1;
        }
    }

#if 0
    if (1) {
        proc_id pid;

        pid = _kern_proc_create_proc("/boot/bin/fortune", "/boot/bin/fortune", NULL, 0, 5);
        if (pid >= 0) {
            int retcode;
            _kern_proc_wait_on_proc(pid, &retcode);
        }
    }

    while (1) {
        proc_id pid;

        pid = _kern_proc_create_proc("/boot/bin/consoled", "/boot/bin/consoled", NULL, 0, 5);
        if (pid >= 0) {
            int retcode;
            printf("init: spawned consoled, pid 0x%x\r\n", pid);
            _kern_proc_wait_on_proc(pid, &retcode);
        }
    }
#endif
    printf("init exiting\r\n");

    return 0;
}


