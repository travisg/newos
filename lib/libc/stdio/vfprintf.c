#if !KERNEL
/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/* Code modified by Justin Smith 2003/06/17 */
#include <stdio.h>
#include <sys/syscalls.h>
#include <errno.h>


static int _write(void* arg, const void* buf, ssize_t len)
{
    int err;
    if(((FILE*)arg)->buf_pos > 0)
    {
        err = _kern_write(((FILE*)arg)->fd, ((FILE*)arg)->buf, -1, ((FILE*)arg)->buf_pos);
        if(err < 0)
        {
            errno = EIO;
            ((FILE*)arg)->flags |= _STDIO_ERROR;
            return err;
        }
        ((FILE*)arg)->buf_pos = 0;
    }
    err = _kern_write(((FILE*)arg)->fd, buf, -1, len);
    if(err < 0)
    {
        errno = EIO;
        ((FILE*)arg)->flags |= _STDIO_ERROR;
    }
    return err;
}

int vfprintf(FILE *stream, char const *format, va_list ap)
{
    return _v_printf(_write, stream, format, ap);
}

#endif
