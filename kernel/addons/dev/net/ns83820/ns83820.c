/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>
#include <string.h>
#include <newos/errors.h>
#include <kernel/net/ethernet.h>

#include "ns83820_priv.h"

static int ns83820_open(dev_ident ident, dev_cookie *cookie)
{
	ns83820 *ns = (ns83820 *)ident;

	*cookie = ns;

	return 0;
}

static int ns83820_freecookie(dev_cookie cookie)
{
	return 0;
}

static int ns83820_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int ns83820_close(dev_cookie cookie)
{
	return 0;
}

static ssize_t ns83820_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	ns83820 *ns = (ns83820 *)cookie;

	if(len < ETHERNET_MAX_SIZE)
		return ERR_VFS_INSUFFICIENT_BUF;
	return ns83820_rx(ns, buf, len);
}

static ssize_t ns83820_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	ns83820 *ns = (ns83820 *)cookie;

	if(len > ETHERNET_MAX_SIZE)
		return ERR_VFS_INSUFFICIENT_BUF;
	if(len < 0)
		return ERR_INVALID_ARGS;

	ns83820_xmit(ns, buf, len);
	return len;
}

static int ns83820_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	ns83820 *ns = (ns83820 *)cookie;
	int err = NO_ERROR;

	dprintf("ns83820_ioctl: op %d, buf %p, len %Ld\n", op, buf, (long long)len);

	if(!ns)
		return ERR_IO_ERROR;

	switch(op) {
		case IOCTL_NET_IF_GET_ADDR: // get the ethernet MAC address
			if(len >= sizeof(ns->mac_addr)) {
				memcpy(buf, ns->mac_addr, sizeof(ns->mac_addr));
			} else {
				err = ERR_VFS_INSUFFICIENT_BUF;
			}
			break;
		case IOCTL_NET_IF_GET_TYPE: // return the type of interface (ethernet, loopback, etc)
			if(len >= sizeof(int)) {
				*(int *)buf = IF_TYPE_ETHERNET;
			} else {
				err = ERR_VFS_INSUFFICIENT_BUF;
			}
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static struct dev_calls ns83820_hooks = {
	&ns83820_open,
	&ns83820_close,
	&ns83820_freecookie,
	&ns83820_seek,
	&ns83820_ioctl,
	&ns83820_read,
	&ns83820_write,
	/* no paging here */
	NULL,
	NULL,
	NULL
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	ns83820 *ns;

	dprintf("ns83820_dev_init: entry\n");

	// detect and setup the device
	if(ns83820_detect(&ns) < 0) {
		// no ns83820 here
		dprintf("ns83820_dev_init: no device found\n");
		return ERR_GENERAL;
	}

	ns83820_init(ns);

	// create device node
	devfs_publish_indexed_device("net/ns83820", ns, &ns83820_hooks);

	return 0;
}

