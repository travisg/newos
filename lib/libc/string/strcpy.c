/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

char *
strcpy(char *dest, char const *src)
{
    char *tmp = dest;

    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

