#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/sem.h>
#include <kernel/arch/cpu.h>

#include <kernel/fs/rootfs.h>

#include <libc/string.h>
#include <libc/ctype.h>

#define MAKE_NOIZE 0

struct vnode {
	struct vnode *next;
	vnode_id id;
	void *priv_vnode;
	struct fs_mount *mount;
	int ref_count;
};

struct file_descriptor {
	struct vnode *vnode;
	void *cookie;
	int ref_count;
};

struct ioctx {
	struct vnode *cwd;
	sem_id io_sem;
	int table_size;
	int num_used_fds;
	struct file_descriptor fds[0];
};

struct fs_container {
	struct fs_container *next;
	struct fs_calls *calls;
	const char *name;
};
static struct fs_container *fs_list;

struct fs_mount {
	struct fs_mount *next;
	struct fs_container *fs;
	fs_id id;
	void *cookie;
	char *mount_point;
	struct vnode root_vnode;
	struct vnode *covers_vnode;
};
static struct fs_mount *fs_mounts;

static fs_id next_fsid = 0;
static sem_id vfs_sem;
static sem_id vfs_mount_sem;
static sem_id vfs_vnode_sem;

/* function declarations */
static int vfs_mount(const char *path, const char *fs_name, bool kernel);
static int vfs_unmount(const char *path);
static int vfs_open(const char *path, const char *stream, stream_type stream_type, bool kernel);
static int vfs_seek(int fd, off_t pos, seek_type seek_type, bool kernel);
static int vfs_read(int fd, void *buf, off_t pos, size_t *len, bool kernel);
static int vfs_write(int fd, const void *buf, off_t pos, size_t *len, bool kernel);
static int vfs_ioctl(int fd, int op, void *buf, size_t len, bool kernel);
static int vfs_close(int fd, bool kernel);
static int vfs_create(const char *path, const char *stream, stream_type stream_type, bool kernel);

#define VNODE_HASH_TABLE_SIZE 1024
static void *vnode_table;
static struct vnode *root_vnode;
static vnode_id next_vnode_id = 0;

static int vnode_compare(void *_v, void *key)
{
	struct vnode *v = _v;

	if(v->priv_vnode == key)
		return 0;
	else
		return -1;
}

static unsigned int vnode_hash(void *_v, void *key, int range)
{
	struct vnode *v = _v;
	
	if(v != NULL)
		return (((addr)v->priv_vnode >> 3) % range);
	else
		return (((addr)key >> 3) % range);
}

static int init_vnode(struct vnode *v)
{
	v->next = NULL;
	v->priv_vnode = NULL;
	v->ref_count = 0;
	
	return 0;
}

static struct vnode *create_new_vnode()
{
	struct vnode *v;
	
	v = (struct vnode *)kmalloc(sizeof(struct vnode));
	if(v == NULL)
		return NULL;
	
	init_vnode(v);
	return v;
}

static int dec_vnode_ref_count(struct vnode *v, bool locked, bool free_mem)
{
	int err;

	if(!locked)
		sem_acquire(vfs_vnode_sem, 1);

	v->ref_count--;
	if(v->ref_count <= 0) {
		hash_remove(vnode_table, v);			
		
		if(!locked)
			sem_release(vfs_vnode_sem, 1);
		v->mount->fs->calls->fs_dispose_vnode(v->mount->cookie, v);
		if(free_mem)
			kfree(v);
		err = 1;
	} else {
		err = 0;
	}
	if(!locked)
		sem_release(vfs_vnode_sem, 1);
	return err;
}

static int inc_vnode_ref_count(struct vnode *v, bool locked)
{
	if(!locked)
		sem_acquire(vfs_vnode_sem, 1);
		
	v->ref_count++;		
	
	if(!locked)		
		sem_release(vfs_vnode_sem, 1);
	return 0;	
}

static int dec_fd_ref_count(struct ioctx *ioctx, int fd, bool locked)
{
	void *cookie = NULL;
	struct vnode *v = NULL;
	int err = 0;

	if(!locked)
		sem_acquire(ioctx->io_sem, 1);

	ioctx->fds[fd].ref_count--;
	if(ioctx->fds[fd].ref_count == 0) {
		cookie = ioctx->fds[fd].cookie;
		v = ioctx->fds[fd].vnode;
	
		ioctx->fds[fd].vnode = NULL;
		ioctx->fds[fd].cookie = NULL;
	}

	if(!locked)
		sem_release(ioctx->io_sem, 1);
		
	if(cookie != NULL) {
		// we need to free it
		err = v->mount->fs->calls->fs_close(v->mount->cookie, v, cookie);
		dec_vnode_ref_count(v, false, true);
	}
	return err;
}

static int inc_fd_ref_count(struct ioctx *ioctx, int fd, bool locked)
{
	if(!locked)
		sem_acquire(ioctx->io_sem, 1);
		
	ioctx->fds[fd].ref_count++;		
	
	if(!locked)
		sem_release(ioctx->io_sem, 1);
	return 0;	
}

static struct fs_container *find_fs(const char *fs_name)
{
	struct fs_container *fs = fs_list;
	while(fs != NULL) {
		if(strcmp(fs_name, fs->name) == 0) {
			return fs;
		}
		fs = fs->next;
	}
	return NULL;
}

static struct fs_mount *find_mount(const char *mnt_point, struct fs_mount **last_mount)
{
	struct fs_mount *mount = fs_mounts;
	struct fs_mount *last = NULL;
	while(mount != NULL) {
		if(strcmp(mnt_point, mount->mount_point) == 0) {
			if(last_mount != NULL)
				*last_mount = last;
			return mount;
		}
		last = mount;
		mount = mount->next;
	}
	return NULL;	
}

#define NEW_FD_TABLE_SIZE 32
void *vfs_new_ioctx()
{
	struct ioctx *ioctx;
	int i;
	
	ioctx = kmalloc(sizeof(struct ioctx) + sizeof(struct file_descriptor) * NEW_FD_TABLE_SIZE);
	if(ioctx == NULL)
		return NULL;
	
	ioctx->io_sem = sem_create(1, "ioctx_sem");
	if(ioctx->io_sem < 0) {
		kfree(ioctx);
		return NULL;
	}
	
	ioctx->cwd = root_vnode;
	ioctx->table_size = NEW_FD_TABLE_SIZE;
	ioctx->num_used_fds = 0;

	for(i=0; i<ioctx->table_size; i++) {
		ioctx->fds[i].vnode = NULL;
		ioctx->fds[i].cookie = NULL;
	}

	return ioctx;
}


int vfs_init(kernel_args *ka)
{
	int err;

	dprintf("vfs_init: entry\n");

	{
		struct vnode *v;
		vnode_table = hash_init(VNODE_HASH_TABLE_SIZE, (addr)&v->next - (addr)v,
			&vnode_compare, &vnode_hash);
		if(vnode_table == NULL)
			panic("vfs_init: error creating vnode hash table\n");
	}
	fs_list = NULL;
	fs_mounts = NULL;
	root_vnode = NULL;

	vfs_sem = sem_create(1, "vfs_lock");
	if(vfs_sem < 0)
		panic("vfs_init: error allocating vfs lock\n");

	vfs_mount_sem = sem_create(1, "vfs_mount_lock");
	if(vfs_mount_sem < 0)
		panic("vfs_init: error allocating vfs_mount lock\n");

	vfs_vnode_sem = sem_create(1, "vfs_vnode_lock");
	if(vfs_vnode_sem < 0)
		panic("vfs_init: error allocating vfs_vnode lock\n");

	// bootstrap the root filesystem
	bootstrap_rootfs();	

	err = sys_mount("/", "rootfs");
	if(err < 0)
		panic("error mounting rootfs!\n");

	return 0;
}

int vfs_test()
{
	int fd;
	int err;
	
	fd = sys_open("/", "", STREAM_TYPE_DIR);
	dprintf("fd = %d\n", fd);
	sys_close(fd);

	fd = sys_open("/", "", STREAM_TYPE_DIR);
	dprintf("fd = %d\n", fd);

	sys_create("/foo", "", STREAM_TYPE_DIR);
	sys_create("/foo/bar", "", STREAM_TYPE_DIR);
	sys_create("/foo/bar/gar", "", STREAM_TYPE_DIR);
	sys_create("/foo/bar/tar", "", STREAM_TYPE_DIR);

	fd = sys_open("/foo/bar", "", STREAM_TYPE_DIR);
	if(fd < 0)
		panic("unable to open /foo/bar\n");

	{
		char buf[64];
		int buf_len;

		sys_seek(fd, 0, SEEK_SET);
		for(;;) {
			buf_len = sizeof(buf);
			err = sys_read(fd, buf, -1, &buf_len);
			if(err < 0)
				panic("readdir returned %d\n", err);
			if(buf_len > 0) 
				dprintf("readdir returned name = '%s'\n", buf);
			else
				break;
		}
	}
#if 1
	sys_create("/test", "", STREAM_TYPE_DIR);
	sys_create("/test", "", STREAM_TYPE_DIR);
	
	err = sys_mount("/test", "rootfs");
	if(err < 0)
		panic("failed mount test\n");

	fd = sys_open("/boot", "", STREAM_TYPE_DIR);
	dprintf("fd = %d\n", fd);
	sys_close(fd);
	
	fd = sys_open("/boot", "", STREAM_TYPE_DIR);
	if(fd < 0)
		panic("unable to open dir /boot\n");
	{
		char buf[64];
		int buf_len;

		sys_seek(fd, 0, SEEK_SET);
		for(;;) {
			buf_len = sizeof(buf);
			err = sys_read(fd, buf, -1, &buf_len);
			if(err < 0)
				panic("readdir returned %d\n", err);
			if(buf_len > 0) 
				dprintf("readdir returned name = '%s'\n", buf);
			else
				break;
		}
	}
#endif	
	return 0;
}

static int get_new_fd(struct ioctx *ioctx)
{
	int i;
	
	for(i=0; i<ioctx->table_size; i++) {
		if(ioctx->fds[i].vnode == NULL) {
			return i;
		}
	}
	return -1;
}

static struct ioctx *get_current_ioctx(bool kernel)
{
	struct ioctx *ioctx;

	if(kernel) {
		ioctx = proc_get_kernel_proc()->ioctx;
	} else {
		ioctx = thread_get_current_thread()->proc->ioctx;
	}
	return ioctx;
}

static struct vnode *get_base_vnode(struct ioctx *ioctx, const char *path, bool kernel)
{
	struct vnode *base_vnode;
	bool get_root_vnode = false;
	int i;
	
	for(i=0; path[i] != '\0'; i++) {
		if(!isspace(path[i])) {
			if(path[i] == '/') {
				get_root_vnode = true;
			}
			break;
		}
	}	
	
	if(get_root_vnode) {
		base_vnode = root_vnode;
		inc_vnode_ref_count(base_vnode, false);
	} else {
		if(ioctx == NULL)
			ioctx = get_current_ioctx(kernel);
	
		sem_acquire(ioctx->io_sem, 1);
	
		base_vnode = ioctx->cwd;
	
		inc_vnode_ref_count(base_vnode, false);
	
		sem_release(ioctx->io_sem, 1);	
	}
	return base_vnode;
}

static int get_vnode_from_fd(int fd, struct ioctx *ioctx, bool kernel, struct vnode **v, void **cookie)
{
	int err;
	
	if(ioctx == NULL)
		ioctx = get_current_ioctx(kernel);

	sem_acquire(ioctx->io_sem, 1);

	if(fd >= ioctx->table_size) {
		err = -1;
		goto err;
	}		

	if(ioctx->fds[fd].vnode == NULL) {
		err = -1;
		goto err;
	}

	*v = ioctx->fds[fd].vnode;
	*cookie = ioctx->fds[fd].cookie;

	inc_vnode_ref_count(*v, false);
	inc_fd_ref_count(ioctx, fd, true);

	err = 0;

err:
	sem_release(ioctx->io_sem, 1);
	return err;
}

static struct vnode *add_vnode_to_list(struct fs_mount *mount, void *priv_vnode)
{
	struct vnode *new_vnode;

	// see if it already exists
	new_vnode = hash_lookup(vnode_table, priv_vnode);
	if(new_vnode == NULL) {
		// it wasn't in the list
		new_vnode = create_new_vnode();
		if(new_vnode == NULL) {
			sem_release(vfs_vnode_sem, 1);
			return NULL;
		}
		new_vnode->priv_vnode = priv_vnode;
		new_vnode->mount = mount;

		hash_insert(vnode_table, new_vnode);
	}
	inc_vnode_ref_count(new_vnode, true);

	return new_vnode;
}

static int add_new_fd(struct ioctx *ioctx, struct vnode *new_vnode, void *cookie)
{
	int fd;
	
	sem_acquire(ioctx->io_sem, 1);

	fd = get_new_fd(ioctx);
	if(fd < 0) {
		sem_release(ioctx->io_sem, 1);
		return -1;
	}

	ioctx->fds[fd].vnode = new_vnode;
	ioctx->fds[fd].cookie = cookie;
	ioctx->fds[fd].ref_count = 0;
	inc_fd_ref_count(ioctx, fd, true);

	sem_release(ioctx->io_sem, 1);

	return fd;
}

int vfs_register_filesystem(const char *name, struct fs_calls *calls)
{
	struct fs_container *container;

	container = (struct fs_container *)kmalloc(sizeof(struct fs_container));
	if(container == NULL)
		return -1;

	container->name = name;
	container->calls = calls;
	
	sem_acquire(vfs_sem, 1);

	container->next = fs_list;
	fs_list = container;
	
	sem_release(vfs_sem, 1);

	return 0;
}

static int vfs_mount(const char *path, const char *fs_name, bool kernel)
{
	struct fs_mount *mount;
	int err = 0;
	struct vnode *covered_vnode = NULL;

#if MAKE_NOIZE
	dprintf("vfs_mount: entry. path = '%s', fs_name = '%s'\n", path, fs_name);
#endif

	sem_acquire(vfs_mount_sem, 1);

	mount = (struct fs_mount *)kmalloc(sizeof(struct fs_mount));
	if(mount == NULL)
		return -1;

	mount->mount_point = (char *)kmalloc(strlen(path)+1);
	if(mount->mount_point == NULL) {
		err = -1;
		goto err;
	}
	strcpy(mount->mount_point, path);

	mount->fs = find_fs(fs_name);
	if(mount->fs == NULL) {
		err = -1;
		goto err1;
	}

	mount->id = next_fsid++;

	if(fs_mounts == NULL) {
		// we haven't mounted anything yet	
		if(strcmp(path, "/") != 0) {
			err = -1;
			goto err1;
		}

		err = init_vnode(&mount->root_vnode);
		if(err < 0)
			goto err1;
		
		err = mount->fs->calls->fs_mount(&mount->cookie, 0, NULL, mount->id, &mount->root_vnode.priv_vnode);
		if(err < 0)
			goto err1;
		root_vnode = &mount->root_vnode;
	} else {
		int fd;
		struct ioctx *ioctx;
		void *null_cookie;
		
		fd = sys_open(path, "", STREAM_TYPE_DIR);
		if(fd < 0) {
			err = fd;
			goto err1;
		}
		// get the vnode this mount will cover
		ioctx = get_current_ioctx(kernel);
		err = get_vnode_from_fd(fd, ioctx, kernel, &covered_vnode, &null_cookie);
		if(err < 0) {
			sys_close(fd);
			goto err1;
		}
		dec_fd_ref_count(ioctx, fd, false);
		sys_close(fd);

		// mount it
		err = mount->fs->calls->fs_mount(&mount->cookie, 0, covered_vnode, mount->id, &mount->root_vnode.priv_vnode);
		if(err < 0)
			goto err1;		
	}

	mount->root_vnode.id = next_vnode_id++;
	mount->root_vnode.ref_count++;
	mount->root_vnode.mount = mount;

	sem_acquire(vfs_sem, 1);
	// insert root vnode into vnode list
	hash_insert(vnode_table, &mount->root_vnode);
	sem_release(vfs_sem, 1);

	// insert mount struct into list
	mount->next = fs_mounts;
	fs_mounts = mount;	

	// if the mount point is on top of another filesystem (will it will be except for the root),
	// register with the underlying fileystem the fact that it's covered and the vnode to
	// redirect to.
	if(covered_vnode != NULL) {
		covered_vnode->mount->fs->calls->fs_register_mountpoint(covered_vnode->mount->cookie,
			covered_vnode->priv_vnode, &mount->root_vnode);
	}

	sem_release(vfs_mount_sem, 1);

	return 0;

err1:
	kfree(mount->mount_point);
err:
	kfree(mount);
	sem_release(vfs_mount_sem, 1);

	return err;
}

static int vfs_unmount(const char *path)
{
	struct fs_mount *mount;
	struct fs_mount *last_mount;
	int err;
	
#if MAKE_NOIZE
	dprintf("vfs_unmount: entry. path = '%s'\n", path);
#endif	
	sem_acquire(vfs_mount_sem, 1);

	mount = find_mount(path, &last_mount);
	if(mount == NULL) {
		err = -1;
		goto err;
	}

	dec_vnode_ref_count(&mount->root_vnode, false, false);

	err = mount->fs->calls->fs_unmount(mount->cookie);
	if(err < 0)
		goto err;

	// remove the mount struct from the list
	if(last_mount != NULL) {
		last_mount = mount->next;
	} else {
		fs_mounts = mount->next;
	}

	sem_release(vfs_mount_sem, 1);

	kfree(mount->mount_point);
	kfree(mount);

	return 0;
	
err:
	sem_release(vfs_mount_sem, 1);
	return err;
}

static int vfs_open(const char *path, const char *stream, stream_type stream_type, bool kernel)
{
	int fd;
	struct ioctx *ioctx;
	struct vnode *base_vnode;
	struct vnode *new_vnode;
	void *v;
	void *cookie;
	int err;
	struct redir_struct redir;
	
#if MAKE_NOIZE	
	dprintf("vfs_open: entry. path = '%s', base_vnode 0x%x\n", path, base_vnode);
#endif	

	ioctx = get_current_ioctx(kernel);
	base_vnode = get_base_vnode(ioctx, path, kernel);

	redir.vnode = base_vnode;
	redir.path = path;
	do {
		struct vnode *curr_base = redir.vnode;
		
		redir.redir = false;
		err = curr_base->mount->fs->calls->fs_open(curr_base->mount->cookie, curr_base->priv_vnode, redir.path, stream, stream_type, &v, &cookie, &redir);	
		if(err < 0)
			goto err;
	} while(redir.redir);
	
	sem_acquire(vfs_vnode_sem, 1);

	new_vnode = add_vnode_to_list(((struct vnode *)redir.vnode)->mount, v);
	if(new_vnode == NULL) {
		sem_release(vfs_vnode_sem, 1);
		err = -1;
		goto err1;
	}

	dec_vnode_ref_count(base_vnode, true, true);

	sem_release(vfs_vnode_sem, 1);

	fd = add_new_fd(ioctx, new_vnode, cookie);
	if(fd < 0) {
		err = -1;
		goto err2;
	}

	return fd;

err2:
	// XXX remove the vnode if recently created
err1:
	// XXX insert fs_close here
err:		
	return err;
}

static int vfs_seek(int fd, off_t pos, seek_type seek_type, bool kernel)
{
	struct ioctx *ioctx;
	struct vnode *vnode;
	void *cookie;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_rewinddir: fd = %d\n", fd);
#endif

	ioctx = get_current_ioctx(kernel);
	err = get_vnode_from_fd(fd, ioctx, kernel, &vnode, &cookie);
	if(err < 0)
		return err;

	err = vnode->mount->fs->calls->fs_seek(vnode->mount->cookie, vnode->priv_vnode, cookie, pos, seek_type);

	dec_fd_ref_count(ioctx, fd, false);
	dec_vnode_ref_count(vnode, false, true);
	
	return err;
}	

static int vfs_read(int fd, void *buf, off_t pos, size_t *len, bool kernel)
{
	struct ioctx *ioctx;
	struct vnode *vnode;
	void *cookie;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_read: fd = %d\n", fd);
#endif

	ioctx = get_current_ioctx(kernel);
	err = get_vnode_from_fd(fd, ioctx, kernel, &vnode, &cookie);
	if(err < 0)
		return err;
	
	err = vnode->mount->fs->calls->fs_read(vnode->mount->cookie, vnode->priv_vnode,
		cookie, buf, pos, len);

	dec_fd_ref_count(ioctx, fd, false);
	dec_vnode_ref_count(vnode, false, true);

	return err;
}

static int vfs_write(int fd, const void *buf, off_t pos, size_t *len, bool kernel)
{
	struct ioctx *ioctx;
	struct vnode *vnode;
	void *cookie;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_write: fd = %d\n", fd);
#endif

	ioctx = get_current_ioctx(kernel);
	err = get_vnode_from_fd(fd, ioctx, kernel, &vnode, &cookie);
	if(err < 0)
		return err;
	
	err = vnode->mount->fs->calls->fs_write(vnode->mount->cookie, vnode->priv_vnode,
		cookie, buf, pos, len);

	dec_fd_ref_count(ioctx, fd, false);
	dec_vnode_ref_count(vnode, false, true);

	return err;
}

static int vfs_ioctl(int fd, int op, void *buf, size_t len, bool kernel)
{
	struct ioctx *ioctx;
	struct vnode *vnode;
	void *cookie;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_ioctl: fd = %d, buf = 0x%x, op = %d\n", fd, buf, op);
#endif

	ioctx = get_current_ioctx(kernel);
	err = get_vnode_from_fd(fd, ioctx, kernel, &vnode, &cookie);
	if(err < 0)
		return err;

	err = vnode->mount->fs->calls->fs_ioctl(vnode->mount->cookie, vnode->priv_vnode,
		cookie, op, buf, len);

	dec_fd_ref_count(ioctx, fd, false);
	dec_vnode_ref_count(vnode, false, true);

	return err;
}

static int vfs_close(int fd, bool kernel)
{
	struct ioctx *ioctx;

	ioctx = get_current_ioctx(kernel);
#if MAKE_NOIZE
	dprintf("vfs_close: fd = %d, ioctx = 0x%x\n", fd, ioctx);
#endif
	return dec_fd_ref_count(ioctx, fd, false);
}

static int vfs_create(const char *path, const char *stream, stream_type stream_type, bool kernel)
{
	struct vnode *base_vnode;
	struct redir_struct redir;
	int err;

	base_vnode = get_base_vnode(NULL, path, kernel);

	redir.vnode = base_vnode;
	redir.path = path;
	do {
		struct vnode *cur_base = redir.vnode;
	
		redir.redir = false;
		err = cur_base->mount->fs->calls->fs_create(cur_base->mount->cookie, cur_base->priv_vnode, redir.path, stream, stream_type, &redir);	
		if(err < 0)
			goto err;
	} while(redir.redir);
	
	dec_vnode_ref_count(base_vnode, false, true);

err:
	return err;
}

int vfs_helper_getnext_in_path(const char *path, int *start_pos, int *end_pos)
{
	int i;

	// skip over leading slashes
	i = *start_pos;
	while(path[i] == '/') {
		i++;
	}
	
	// check to see if we walked to the end of a path
	if(path[i] == '\0') {
		if(i == *start_pos) {
			return 0; // we didn't move at all, thus no path components were found	
		} else {
			*end_pos = i;
			*start_pos = i;
			return 1;
		}
	}
	*start_pos = i;

	// skip over anything else
	while(path[i] != '/' && path[i] != '\0') {
		i++;
	}

	// even if we didn't move at all, we found a path component
	*end_pos = i;
	
	return 1;
}

int sys_mount(const char *path, const char *fs_name)
{
	return vfs_mount(path, fs_name, true);
}

int sys_unmount(const char *path)
{
	return vfs_unmount(path);
}

int sys_open(const char *path, const char *stream, stream_type stream_type)
{
	return vfs_open(path, stream, stream_type, true);
}

int sys_seek(int fd, off_t pos, seek_type seek_type)
{
	return vfs_seek(fd, pos, seek_type, true);
}

int sys_read(int fd, void *buf, off_t pos, size_t *len)
{
	return vfs_read(fd, buf, pos, len, true);
}

int sys_write(int fd, const void *buf, off_t pos, size_t *len)
{
	return vfs_write(fd, buf, pos, len, true);
}

int sys_ioctl(int fd, int op, void *buf, size_t len)
{
	return vfs_ioctl(fd, op, buf, len, true);
}

int sys_close(int fd)
{
	return vfs_close(fd, true);
}

int sys_create(const char *path, const char *stream, stream_type stream_type)
{
	return vfs_create(path, stream, stream_type, true);
}

int user_open(const char *path, const char *stream, stream_type stream_type)
{
	return vfs_open(path, stream, stream_type, false);
}

int user_seek(int fd, off_t pos, seek_type seek_type)
{
	return vfs_seek(fd, pos, seek_type, false);
}

int user_read(int fd, void *buf, off_t pos, size_t *len)
{
	return vfs_read(fd, buf, pos, len, false);
}

int user_write(int fd, const void *buf, off_t pos, size_t *len)
{
	return vfs_write(fd, buf, pos, len, false);
}

int user_ioctl(int fd, int op, void *buf, size_t len)
{
	return vfs_ioctl(fd, op, buf, len, false);
}

int user_close(int fd)
{
	return vfs_close(fd, false);
}

int user_create(const char *path, const char *stream, stream_type stream_type)
{
	return vfs_create(path, stream, stream_type, false);
}



