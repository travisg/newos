/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _DEVFS_H
#define _DEVFS_H

#include <kernel/vfs.h>

int bootstrap_devfs();

typedef void * dev_cookie;

struct dev_calls {
	int (*dev_open)(const char *name, dev_cookie *cookie);
	int (*dev_close)(dev_cookie cookie);
	int (*dev_freecookie)(dev_cookie cookie);

	int (*dev_seek)(dev_cookie cookie, off_t pos, seek_type st);
	int (*dev_ioctl)(dev_cookie cookie, int op, void *buf, size_t len);

	ssize_t (*dev_read)(dev_cookie cookie, void *buf, off_t pos, ssize_t len);
	ssize_t (*dev_write)(dev_cookie cookie, const void *buf, off_t pos, ssize_t len);

	int (*dev_canpage)();
	ssize_t (*dev_readpage)(iovecs *vecs, off_t pos);
	ssize_t (*dev_writepage)(iovecs *vecs, off_t pos);
};

/* api drivers will use to publish devices */
int devfs_publish_device(const char *path, struct dev_calls *calls);

#endif
