/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <libc/string.h>
#include <sys/errors.h>

#include "rtl8139_priv.h"

struct rtl8139_fs {
	fs_id id;
	void *covered_vnode;
	void *redir_vnode;
	rtl8139 *rtl;
	int root_vnode; // just a placeholder to return a pointer to
};

static int rtl8139_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct rtl8139_fs *fs = _fs;
	void *redir_vnode = fs->redir_vnode;
	
	if(redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = redir_vnode;
		redir->path = path;
		return 0;
	}
	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE)
		return ERR_VFS_PATH_NOT_FOUND;

	*_vnode = &fs->root_vnode;	
	*_cookie = NULL;

	return 0;
}

static int rtl8139_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	return ERR_NOT_ALLOWED;
}

static int rtl8139_close(void *_fs, void *_vnode, void *_cookie)
{
	return 0;
}

static int rtl8139_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct rtl8139_fs *fs = _fs;
	void *redir_vnode = fs->redir_vnode;
	
	if(redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = redir_vnode;
		redir->path = path;
		return 0;
	}
	
	return ERR_VFS_READONLY_FS;
}

static int rtl8139_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct rtl8139_fs *fs = _fs;
	void *redir_vnode = fs->redir_vnode;
	
	if(redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = redir_vnode;
		redir->path = path;
		return 0;
	}
	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE)
		return ERR_VFS_PATH_NOT_FOUND;

	stat->size = 0;
	return 0;
 }

static int rtl8139_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	struct rtl8139_fs *fs = _fs;
	int err;
	
	if(*len < 1500)
		return ERR_VFS_INSUFFICIENT_BUF;
	if(fs->rtl == NULL)
		return ERR_IO_ERROR;
	err = rtl8139_rx(fs->rtl, buf, *len);
	if(err < 0)
		return err;
	else
		*len = err;
	return 0;
}

static int rtl8139_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	struct rtl8139_fs *fs = _fs;

	if(*len > 1500)
		return ERR_VFS_INSUFFICIENT_BUF;
	if(fs->rtl == NULL)
		return ERR_IO_ERROR;

	rtl8139_xmit(fs->rtl, buf, *len);
	return 0;
}

static int rtl8139_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	struct rtl8139_fs *fs = _fs;
	int err;

	dprintf("rtl8139_ioctl: op %d, buf 0x%x, len %d\n", op, buf, len);

	switch(op) {
		case 10000: // get the ethernet MAC address
			if(len >= sizeof(fs->rtl->mac_addr)) {
				memcpy(buf, fs->rtl->mac_addr, sizeof(fs->rtl->mac_addr));
				err = 0;
			} else {
				err = ERR_VFS_INSUFFICIENT_BUF;
			}
			break;
		case 87912: // set the rtl pointer
			fs->rtl = buf;
			err = 0;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}
	
	
	return err;
}

static int rtl8139_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct rtl8139_fs *fs;

	fs = kmalloc(sizeof(struct rtl8139_fs));
	if(fs == NULL)
		return ERR_NO_MEMORY;

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;
	fs->rtl = NULL;

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;
}

static int rtl8139_unmount(void *_fs)
{
	struct rtl8139_fs *fs = _fs;

	kfree(fs);

	return 0;	
}

static int rtl8139_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct rtl8139_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int rtl8139_unregister_mountpoint(void *_fs, void *_v)
{
	struct rtl8139_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int rtl8139_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

static struct fs_calls rtl8139_hooks = {
	&rtl8139_mount,
	&rtl8139_unmount,
	&rtl8139_register_mountpoint,
	&rtl8139_unregister_mountpoint,
	&rtl8139_dispose_vnode,
	&rtl8139_open,
	&rtl8139_seek,
	&rtl8139_read,
	&rtl8139_write,
	&rtl8139_ioctl,
	&rtl8139_close,
	&rtl8139_create,
	&rtl8139_stat
};

int rtl8139_dev_init(kernel_args *ka)
{
	rtl8139 *rtl;
	int fd;
	
	dprintf("rtl8139_dev_init: entry\n");

	// detect and setup the device
	if(rtl8139_detect(&rtl) < 0) {
		// no rtl8139 here
		dprintf("rtl8139_dev_init: no device found\n");
		return 0;
	}
	
	rtl8139_init(rtl);

#if 1
	// create device node
	vfs_register_filesystem("rtl8139_dev_fs", &rtl8139_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/net", "", STREAM_TYPE_DIR);
	sys_create("/dev/net/rtl8139", "", STREAM_TYPE_DIR);
	sys_create("/dev/net/rtl8139/0", "", STREAM_TYPE_DIR);
	sys_mount("/dev/net/rtl8139/0", "rtl8139_dev_fs");

	fd = sys_open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
	if(fd < 0) {
		panic("rtl8139_dev_init: could not open %s\n", "/dev/net/rtl8139/0");
	}
	sys_ioctl(fd, 87912, (void *)rtl, 4);
	sys_close(fd);
#endif
	return 0;
}

