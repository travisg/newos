/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

char *
strchr(const char *s, int c)
{
    for (; *s != (char) c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *) s;
}
