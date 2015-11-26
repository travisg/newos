/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int rc;

    if (argc < 2) {
        printf("not enough arguments to unmount\n");
        return 0;
    }

    rc = _kern_unmount(argv[1]);
    if (rc < 0) {
        printf("_kern_unmount() returned error: %s\n", strerror(rc));
    } else {
        printf("%s successfully unmounted.\n", argv[1]);
    }

    return 0;
}

