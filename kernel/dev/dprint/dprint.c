/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>
#include <kernel/vm.h>
#include <kernel/dev/fixed.h>
#include <newos/errors.h>

static int dprint_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = NULL;
	return 0;
}

static int dprint_close(dev_cookie cookie)
{
	return 0;
}

static int dprint_freecookie(dev_cookie cookie)
{
	return 0;
}

static int dprint_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int dprint_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t dprint_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t dprint_write(dev_cookie cookie, const void *inbuf, off_t pos, ssize_t len)
{
	char buf[256];
	ssize_t bytes_written = 0;
	int err;

	while(len > 0) {
		int towrite = min((int)sizeof(buf), len);

		err = user_memcpy(buf, inbuf, towrite);
		if(err < 0)
			return err;

		dbg_write(buf, towrite);

		len -= towrite;
		bytes_written += towrite;
	}

	return bytes_written;
}

static struct dev_calls dprint_hooks = {
	&dprint_open,
	&dprint_close,
	&dprint_freecookie,
	&dprint_seek,
	&dprint_ioctl,
	&dprint_read,
	&dprint_write,
	/* no paging from /dev/dprint */
	NULL,
	NULL,
	NULL
};

int dprint_dev_init(kernel_args *ka)
{
	// create device node
	devfs_publish_device("dprint", NULL, &dprint_hooks);

	return 0;
}

