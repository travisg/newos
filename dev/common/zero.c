/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <libc/string.h>
#include <sys/errors.h>

struct zero_fs {
	fs_id id;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

static int zero_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct zero_fs *fs = _fs;
	void *redir_vnode = fs->redir_vnode;
	
	if(redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = redir_vnode;
		redir->path = path;
		return 0;
	}		

	*_vnode = &fs->root_vnode;	
	*_cookie = NULL;

	return 0;
}

static int zero_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	return ERR_NOT_ALLOWED;
}

static int zero_close(void *_fs, void *_vnode, void *_cookie)
{
	return 0;
}

static int zero_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct zero_fs *fs = _fs;
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

static int zero_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct zero_fs *fs = _fs;
	void *redir_vnode = fs->redir_vnode;
	
	if(redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = redir_vnode;
		redir->path = path;
		return 0;
	}

	stat->size = 0;
	return 0;
}

static int zero_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	memset(buf, 0, *len);
	return 0;
}

static int zero_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return ERR_VFS_READONLY_FS;
}

static int zero_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

static int zero_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct zero_fs *fs;

	fs = kmalloc(sizeof(struct zero_fs));
	if(fs == NULL)
		return ERR_NO_MEMORY;

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;
}

static int zero_unmount(void *_fs)
{
	struct zero_fs *fs = _fs;

	kfree(fs);

	return 0;	
}

static int zero_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct zero_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int zero_unregister_mountpoint(void *_fs, void *_v)
{
	struct zero_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int zero_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

static struct fs_calls zero_hooks = {
	&zero_mount,
	&zero_unmount,
	&zero_register_mountpoint,
	&zero_unregister_mountpoint,
	&zero_dispose_vnode,
	&zero_open,
	&zero_seek,
	&zero_read,
	&zero_write,
	&zero_ioctl,
	&zero_close,
	&zero_create,
	&zero_stat,
};

int zero_dev_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("zero_dev_fs", &zero_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/zero", "", STREAM_TYPE_DIR);
	sys_mount("/dev/zero", "zero_dev_fs");

	return 0;
}

