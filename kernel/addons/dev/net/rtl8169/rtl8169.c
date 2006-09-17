/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
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

#include "rtl8169_priv.h"

static int rtl8169_open(dev_ident ident, dev_cookie *cookie)
{
	rtl8169 *r = (rtl8169 *)ident;

	*cookie = r;

	return 0;
}

static int rtl8169_freecookie(dev_cookie cookie)
{
	return 0;
}

static int rtl8169_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int rtl8169_close(dev_cookie cookie)
{
	return 0;
}

static ssize_t rtl8169_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	rtl8169 *r = (rtl8169 *)cookie;

	if(len < ETHERNET_MAX_SIZE)
		return ERR_VFS_INSUFFICIENT_BUF;
	return rtl8169_rx(r, buf, len);
}

static ssize_t rtl8169_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	rtl8169 *r = (rtl8169 *)cookie;

	if(len > ETHERNET_MAX_SIZE)
		return ERR_VFS_INSUFFICIENT_BUF;
	if(len < 0)
		return ERR_INVALID_ARGS;

	rtl8169_xmit(r, buf, len);
	return len;
}

static int rtl8169_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	rtl8169 *r = (rtl8169 *)cookie;
	int err = NO_ERROR;

	dprintf("rtl8169_ioctl: op %d, buf %p, len %Ld\n", op, buf, (long long)len);

	if(!r)
		return ERR_IO_ERROR;

	switch(op) {
		case IOCTL_NET_IF_GET_ADDR: // get the ethernet MAC address
			if(len >= sizeof(r->mac_addr)) {
				memcpy(buf, r->mac_addr, sizeof(r->mac_addr));
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

static struct dev_calls rtl8169_hooks = {
	&rtl8169_open,
	&rtl8169_close,
	&rtl8169_freecookie,
	&rtl8169_seek,
	&rtl8169_ioctl,
	&rtl8169_read,
	&rtl8169_write,
	/* no paging here */
	NULL,
	NULL,
	NULL
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	rtl8169 *rtl8169_list;
	rtl8169 *r;
	int err;

	dprintf("rtl8169_dev_init: entry\n");

	// detect and setup the device
	if(rtl8169_detect(&rtl8169_list) < 0) {
		// no rtl8169 here
		dprintf("rtl8169_dev_init: no devices found\n");
		return ERR_GENERAL;
	}

	// initialize each of the nodes
	for (r = rtl8169_list; r; r = r->next) {
		err = rtl8169_init(r);
		if (err < NO_ERROR)
			continue;

		// create device node
		devfs_publish_indexed_device("net/rtl8169", r, &rtl8169_hooks);
	}

//	dprintf("spinning forever\n");
//	for(;;);

	return 0;
}

