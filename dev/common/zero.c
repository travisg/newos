#include <kernel.h>
#include <stage2.h>
#include <vm.h>
#include <vfs.h>
#include <string.h>

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
	return -1;
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
	
	return -1;
}

static int zero_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	memset(buf, 0, *len);
	return 0;
}

static int zero_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return -1;
}

static int zero_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return -1;
}

static int zero_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct zero_fs *fs;

	fs = kmalloc(sizeof(struct zero_fs));
	if(fs == NULL)
		return -1;

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
};

int zero_dev_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("zero_dev_fs", &zero_hooks);
	vfs_create(NULL, "/dev", "", STREAM_TYPE_DIR);
	vfs_create(NULL, "/dev/zero", "", STREAM_TYPE_DIR);
	vfs_mount("/dev/zero", "zero_dev_fs");

	return 0;
}

