/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/khash.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/debug.h>

#include <string.h>

#include "zfs.h"

// unused
#if 0
static int zfs_read_superblock(zfs_fs *fs)
{
	return 0;
}
#endif

int zfs_mount(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid)
{
	zfs_fs *zfs;
	int err;

	dprintf("zfs_mount: device %s\n", device);

	zfs = kmalloc(sizeof(zfs_fs));
	if(!zfs) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	memset(zfs, 0, sizeof(zfs_fs));

	zfs->id = id;

	zfs->fd = sys_open(device, 0);
	if(zfs->fd < 0) {
		err = zfs->fd;
		goto err1;
	}

	err = vfs_get_vnode_from_fd(zfs->fd, true, &zfs->dev_vnode);
	if(err < 0)
		goto err2;

	// see if we can page from this device
	if(!vfs_canpage(zfs->dev_vnode)) {
		err = ERR_VFS_WRONG_STREAM_TYPE;
		goto err3;
	}

	*fs = zfs;

	return 0;

err3:
	vfs_put_vnode_ptr(zfs->dev_vnode);
err2:
	sys_close(zfs->fd);
err1:
	kfree(zfs);
err:
	return err;
}

int zfs_unmount(fs_cookie fs)
{
	zfs_fs *zfs = (zfs_fs *)fs;

	vfs_put_vnode_ptr(zfs->dev_vnode);
	sys_close(zfs->fd);

	kfree(zfs);

	return 0;
}

int zfs_sync(fs_cookie fs)
{
	return 0;
}

static struct fs_calls zfs_calls = {
	&zfs_mount,
	&zfs_unmount,
	&zfs_sync,

	&zfs_lookup,

	&zfs_getvnode,
	&zfs_putvnode,
	&zfs_removevnode,

	&zfs_opendir,
	&zfs_closedir,
	&zfs_rewinddir,
	&zfs_readdir,

	&zfs_open,
	&zfs_close,
	&zfs_freecookie,
	&zfs_fsync,

	&zfs_read,
	&zfs_write,
	&zfs_seek,
	&zfs_ioctl,

	&zfs_canpage,
	&zfs_readpage,
	&zfs_writepage,

	&zfs_create,
	&zfs_unlink,
	&zfs_rename,

	&zfs_mkdir,
	&zfs_rmdir,

	&zfs_rstat,
	&zfs_wstat
};

int fs_bootstrap(void);
int fs_bootstrap(void)
{
	dprintf("bootstrap_zfs: entry\n");
	return vfs_register_filesystem("zfs", &zfs_calls);
}

