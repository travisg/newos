/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>
#include <sys/errors.h>

static int null_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = NULL;
	return 0;
}

static int null_close(dev_cookie cookie)
{
	return 0;
}

static int null_freecookie(dev_cookie cookie)
{
	return 0;
}

static int null_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int null_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t null_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	return 0;
}

static ssize_t null_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	return len;
}

static struct dev_calls null_hooks = {
	&null_open,
	&null_close,
	&null_freecookie,
	&null_seek,
	&null_ioctl,
	&null_read,
	&null_write,
	/* no paging from /dev/null */
	NULL,
	NULL,
	NULL
};

int null_dev_init(kernel_args *ka)
{
	// create device node
	devfs_publish_device("null", NULL, &null_hooks);

	return 0;
}

