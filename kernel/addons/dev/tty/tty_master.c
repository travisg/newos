/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>

#include <newos/tty_priv.h>

#include "tty_priv.h"

#define TTY_MASTER_TRACE TTY_TRACE

#if TTY_MASTER_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

typedef struct tty_master_cookie {
    tty_desc *tty;
} tty_master_cookie;

static int ttym_open(dev_ident ident, dev_cookie *_cookie)
{
    tty_master_cookie *cookie;

    cookie = (tty_master_cookie *)kmalloc(sizeof(tty_master_cookie));
    if (cookie == NULL)
        return ERR_NO_MEMORY;

    cookie->tty = allocate_new_tty();
    if (cookie->tty == NULL) {
        dprintf("ttym_open: couldn't allocate new tty!\n");
        kfree(cookie);
        return ERR_NO_MORE_HANDLES;
    }

    TRACE(("ttym_open: allocated tty %d, cookie %p\n", cookie->tty->index, cookie));

    *_cookie = cookie;
    return 0;
}

static int ttym_close(dev_cookie _cookie)
{
    TRACE(("ttym_close: cookie %p\n", _cookie));

    return 0;
}

static int ttym_freecookie(dev_cookie _cookie)
{
    tty_master_cookie *cookie = (tty_master_cookie *)_cookie;

    dec_tty_ref(cookie->tty);
    kfree(cookie);

    return 0;
}

static int ttym_seek(dev_cookie _cookie, off_t pos, seek_type st)
{
    return ERR_NOT_ALLOWED;
}

static int ttym_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
    tty_master_cookie *cookie = (tty_master_cookie *)_cookie;
    int err;

    TRACE(("ttym_ioctl: cookie %p, op %d, buf %p, len %d\n", cookie, op, buf, len));

    switch (op) {
        case _TTY_IOCTL_GET_TTY_NUM:
            err = cookie->tty->index;
            break;
        default:
            err = tty_ioctl(cookie->tty, op, buf, len);
    }

    return err;
}

static ssize_t ttym_read(dev_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
    tty_master_cookie *cookie = (tty_master_cookie *)_cookie;
    ssize_t ret;

    TRACE(("ttym_read: tty %d cookie %p, buf %p, len %d\n", cookie->tty->index, cookie, buf, len));

    ret = tty_read(cookie->tty, buf, len, ENDPOINT_MASTER_READ);

    TRACE(("ttym_read: tty %d returns %d\n", cookie->tty->index, ret));

    return ret;
}

static ssize_t ttym_write(dev_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
    tty_master_cookie *cookie = (tty_master_cookie *)_cookie;
    ssize_t ret;

    TRACE(("ttym_write: tty %d cookie %p, buf %p, len %d\n", cookie->tty->index, cookie, buf, len));

    ret = tty_write(cookie->tty, buf, len, ENDPOINT_MASTER_WRITE);

    TRACE(("ttym_write: tty %d returns %d\n", cookie->tty->index, ret));

    return ret;
}

struct dev_calls ttym_hooks = {
    &ttym_open,
    &ttym_close,
    &ttym_freecookie,
    &ttym_seek,
    &ttym_ioctl,
    &ttym_read,
    &ttym_write,
    /* cannot page from /dev/tty */
    NULL,
    NULL,
    NULL
};

