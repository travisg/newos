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

#include "rhine_priv.h"

static int rhine_open(dev_ident ident, dev_cookie *cookie)
{
    rhine *r = (rhine *)ident;

    *cookie = r;

    return 0;
}

static int rhine_freecookie(dev_cookie cookie)
{
    return 0;
}

static int rhine_seek(dev_cookie cookie, off_t pos, seek_type st)
{
    return ERR_NOT_ALLOWED;
}

static int rhine_close(dev_cookie cookie)
{
    return 0;
}

static ssize_t rhine_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
    rhine *r = (rhine *)cookie;

    if (len < ETHERNET_MAX_SIZE)
        return ERR_VFS_INSUFFICIENT_BUF;
    return rhine_rx(r, buf, len);
}

static ssize_t rhine_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
    rhine *r = (rhine *)cookie;

    if (len > ETHERNET_MAX_SIZE)
        return ERR_VFS_INSUFFICIENT_BUF;
    if (len < 0)
        return ERR_INVALID_ARGS;

    rhine_xmit(r, buf, len);
    return len;
}

static int rhine_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
    rhine *r = (rhine *)cookie;
    int err = NO_ERROR;

    dprintf("rhine_ioctl: op %d, buf %p, len %Ld\n", op, buf, (long long)len);

    if (!r)
        return ERR_IO_ERROR;

    switch (op) {
        case IOCTL_NET_IF_GET_ADDR: // get the ethernet MAC address
            if (len >= sizeof(r->mac_addr)) {
                memcpy(buf, r->mac_addr, sizeof(r->mac_addr));
            } else {
                err = ERR_VFS_INSUFFICIENT_BUF;
            }
            break;
        case IOCTL_NET_IF_GET_TYPE: // return the type of interface (ethernet, loopback, etc)
            if (len >= sizeof(int)) {
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

static struct dev_calls rhine_hooks = {
    &rhine_open,
    &rhine_close,
    &rhine_freecookie,
    &rhine_seek,
    &rhine_ioctl,
    &rhine_read,
    &rhine_write,
    /* no paging here */
    NULL,
    NULL,
    NULL
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
    rhine *rhine_list;
    rhine *r;
    int err;

    dprintf("rhine_dev_init: entry\n");

    // detect and setup the device
    if (rhine_detect(&rhine_list) < 0) {
        // no rhine here
        dprintf("rhine_dev_init: no devices found\n");
        return ERR_GENERAL;
    }

    // initialize each of the nodes
    for (r = rhine_list; r; r = r->next) {
        err = rhine_init(r);
        if (err < NO_ERROR)
            continue;

        // create device node
        devfs_publish_indexed_device("net/rhine", r, &rhine_hooks);
    }

//  dprintf("spinning forever\n");
//  for(;;);

    return 0;
}

