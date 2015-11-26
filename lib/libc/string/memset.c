/*
** Copyright 2005, Michael Noisternig. All rights reserved.
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <sys/types.h>

void *
memset(void *s, int c, size_t count)
{
    char *xs = (char *) s;
    size_t len = (-(size_t)s) & (sizeof(size_t)-1);
    int cc = c & 0xff;

    if ( count > len ) {
        count -= len;
        cc |= cc << 8;
        cc |= cc << 16;

        // write to non-aligned memory byte-wise
        for ( ; len > 0; len-- )
            *xs++ = c;

        // write to aligned memory dword-wise
        for ( len = count/sizeof(size_t); len > 0; len-- ) {
            *(size_t *)xs = cc;
            xs += sizeof(size_t);
        }

        count &= sizeof(size_t)-1;
    }

    // write remaining bytes
    for ( ; count > 0; count-- )
        *xs++ = c;

    return s;
}
