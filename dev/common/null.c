/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <sys/errors.h>

struct null_fs {
	fs_id id;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

static int null_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct null_fs *fs = _fs;
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

static int null_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	return ERR_NOT_ALLOWED;
}

static int null_close(void *_fs, void *_vnode, void *_cookie)
{
	return 0;
}

static int null_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct null_fs *fs = _fs;
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

static int null_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct null_fs *fs = _fs;
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
	
static int null_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	*len = 0;
	return 0;
}

static int null_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return 0;
}

static int null_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

static int null_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct null_fs *fs;

	fs = kmalloc(sizeof(struct null_fs));
	if(fs == NULL)
		return ERR_NO_MEMORY;

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;
}

static int null_unmount(void *_fs)
{
	struct null_fs *fs = _fs;

	kfree(fs);

	return 0;	
}

static int null_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct null_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int null_unregister_mountpoint(void *_fs, void *_v)
{
	struct null_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int null_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

static struct fs_calls null_hooks = {
	&null_mount,
	&null_unmount,
	&null_register_mountpoint,
	&null_unregister_mountpoint,
	&null_dispose_vnode,
	&null_open,
	&null_seek,
	&null_read,
	&null_write,
	&null_ioctl,
	&null_close,
	&null_create,
	&null_stat,
};

int null_dev_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("null_dev_fs", &null_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/null", "", STREAM_TYPE_DIR);
	sys_mount("/dev/null", "null_dev_fs");

	return 0;
}

