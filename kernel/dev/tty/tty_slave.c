/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>

#include <newos/tty_priv.h>

#include "tty_priv.h"

#define TTY_SLAVE_TRACE TTY_TRACE

#if TTY_SLAVE_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

typedef struct tty_slave_cookie {
	tty_desc *tty;
} tty_slave_cookie;

static int ttys_open(dev_ident ident, dev_cookie *_cookie)
{
	tty_slave_cookie *cookie;
	tty_desc *tty = (tty_desc *)ident;

	inc_tty_ref(tty);

	cookie = (tty_slave_cookie *)kmalloc(sizeof(tty_slave_cookie));
	if(cookie == NULL)
		return ERR_NO_MEMORY;	

	cookie->tty = tty;

	TRACE(("ttys_open: opened tty %d, cookie %p\n", cookie->tty->index, cookie));

	*_cookie = cookie;
	return 0;
}

static int ttys_close(dev_cookie _cookie)
{
	TRACE(("ttys_close: cookie %p\n", _cookie));

	return 0;
}

static int ttys_freecookie(dev_cookie _cookie)
{
	tty_slave_cookie *cookie = (tty_slave_cookie *)_cookie;

	dec_tty_ref(cookie->tty);
	kfree(cookie);

	return 0;
}

static int ttys_seek(dev_cookie _cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int ttys_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	tty_slave_cookie *cookie = (tty_slave_cookie *)_cookie;
	int err;

	TRACE(("ttys_ioctl: cookie %p, op %d, buf %p, len %d\n", cookie, op, buf, len));

	switch(op) {
		case _TTY_IOCTL_GET_TTY_NUM:
			err = cookie->tty->index;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static ssize_t ttys_read(dev_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
	tty_slave_cookie *cookie = (tty_slave_cookie *)_cookie;
	ssize_t ret;

	TRACE(("ttys_read: cookie %p, buf %p, len %d\n", cookie, buf, len));

	ret = tty_read(cookie->tty, buf, len, ENDPOINT_SLAVE_READ);

	TRACE(("ttys_read: returns %d\n", ret));	

	return ret;
}

static ssize_t ttys_write(dev_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
	tty_slave_cookie *cookie = (tty_slave_cookie *)_cookie;
	ssize_t ret;

	TRACE(("ttys_write: cookie %p, buf %p, len %d\n", cookie, buf, len));

	ret = tty_write(cookie->tty, buf, len, ENDPOINT_SLAVE_WRITE);

	TRACE(("ttys_write: returns %d\n", ret));	

	return ret;
}


struct dev_calls ttys_hooks = {
	&ttys_open,
	&ttys_close,
	&ttys_freecookie,
	&ttys_seek,
	&ttys_ioctl,
	&ttys_read,
	&ttys_write,
	/* cannot page from /dev/tty */
	NULL,
	NULL,
	NULL
};

