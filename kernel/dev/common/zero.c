/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>
#include <kernel/vm.h>
#include <kernel/dev/common/zero.h>
#include <nulibc/string.h>
#include <sys/errors.h>

static int zero_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = NULL;
	return 0;
}

static int zero_close(dev_cookie cookie)
{
	return 0;
}

static int zero_freecookie(dev_cookie cookie)
{
	return 0;
}

static int zero_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int zero_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t zero_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	int rc;

	rc = user_memset(buf, 0, len);
	if(rc < 0)
		return rc;

	return len;
}

static ssize_t zero_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	return 0;
}

static struct dev_calls zero_hooks = {
	&zero_open,
	&zero_close,
	&zero_freecookie,
	&zero_seek,
	&zero_ioctl,
	&zero_read,
	&zero_write,
	/* no paging from /dev/zero */
	NULL,
	NULL,
	NULL
};

int zero_dev_init(kernel_args *ka)
{
	// create device node
	devfs_publish_device("zero", NULL, &zero_hooks);

	return 0;
}

