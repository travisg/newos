/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/vfs.h>
#include <kernel/vm.h>
#include <kernel/vm_cache.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/lock.h>
#include <kernel/thread.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>
#include <kernel/elf.h>
#include <kernel/fs/rootfs.h>
#include <kernel/fs/bootfs.h>
#include <kernel/fs/devfs.h>
#include <sys/errors.h>

#include <kernel/fs/rootfs.h>

#include <libc/string.h>
#include <libc/printf.h>
#include <libc/ctype.h>

#define MAKE_NOIZE 0

struct vnode {
	struct vnode *next;
	struct vnode *mount_prev;
	struct vnode *mount_next;
	struct vm_cache *cache;
	fs_id fsid;
	vnode_id vnid;
	fs_vnode priv_vnode;
	struct fs_mount *mount;
	struct vnode *covered_by;
	int ref_count;
	bool delete_me;
	bool busy;
};
struct vnode_hash_key {
	fs_id fsid;
	vnode_id vnid;
};

struct file_descriptor {
	struct vnode *vnode;
	file_cookie cookie;
	int ref_count;
};

struct ioctx {
	struct vnode *cwd;
	mutex io_mutex;
	int table_size;
	int num_used_fds;
	struct file_descriptor *fds[0];
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
	void *fscookie;
	char *mount_point;
	recursive_lock rlock;
	struct vnode *root_vnode;
	struct vnode *covers_vnode;
	struct vnode *vnodes_head;
	struct vnode *vnodes_tail;
	bool unmounting;
};

static mutex vfs_mutex;
static mutex vfs_mount_mutex;
static mutex vfs_mount_op_mutex;
static mutex vfs_vnode_mutex;

/* function declarations */
static int vfs_mount(char *path, const char *device, const char *fs_name, void *args, bool kernel);
static int vfs_unmount(char *path, bool kernel);
static int vfs_open(char *path, stream_type st, int omode, bool kernel);
static int vfs_seek(int fd, off_t pos, seek_type seek_type, bool kernel);
static ssize_t vfs_read(int fd, void *buf, off_t pos, ssize_t len, bool kernel);
static ssize_t vfs_write(int fd, const void *buf, off_t pos, ssize_t len, bool kernel);
static int vfs_ioctl(int fd, int op, void *buf, size_t len, bool kernel);
static int vfs_close(int fd, bool kernel);
static int vfs_create(char *path, stream_type stream_type, void *args, bool kernel);
static int vfs_rstat(char *path, struct file_stat *stat, bool kernel);

#define VNODE_HASH_TABLE_SIZE 1024
static void *vnode_table;
static struct vnode *root_vnode;
static vnode_id next_vnode_id = 0;

#define MOUNTS_HASH_TABLE_SIZE 16
static void *mounts_table;
static fs_id next_fsid = 0;

static int mount_compare(void *_m, void *_key)
{
	struct fs_mount *mount = _m;
	fs_id *id = _key;

	if(mount->id == *id)
		return 0;
	else
		return -1;
}

static unsigned int mount_hash(void *_m, void *_key, int range)
{
	struct fs_mount *mount = _m;
	fs_id *id = _key;

	if(mount)
		return mount->id % range;
	else
		return *id % range;
}

static struct fs_mount *fsid_to_mount(fs_id id)
{
	struct fs_mount *mount;

	mutex_lock(&vfs_mount_mutex);

	mount = hash_lookup(mounts_table, &id);

	mutex_unlock(&vfs_mount_mutex);

	return mount;
}

static int vnode_compare(void *_v, void *_key)
{
	struct vnode *v = _v;
	struct vnode_hash_key *key = _key;

	if(v->fsid == key->fsid && v->vnid == key->vnid)
		return 0;
	else
		return -1;
}

static unsigned int vnode_hash(void *_v, void *_key, int range)
{
	struct vnode *v = _v;
	struct vnode_hash_key *key = _key;

#define VHASH(fsid, vnid) (((uint32)((vnid)>>32) + (uint32)(vnid)) ^ (uint32)(fsid))

	if(v != NULL)
		return (VHASH(v->fsid, v->vnid) % range);
	else
		return (VHASH(key->fsid, key->vnid) % range);

#undef VHASH
}

static int init_vnode(struct vnode *v)
{
	v->next = NULL;
	v->mount_prev = NULL;
	v->mount_next = NULL;
	v->priv_vnode = NULL;
	v->cache = NULL;
	v->ref_count = 0;
	v->vnid = 0;
	v->fsid = 0;
	v->delete_me = false;
	v->busy = false;
	v->covered_by = NULL;
	v->mount = NULL;

	return 0;
}

static void add_vnode_to_mount_list(struct vnode *v, struct fs_mount *mount)
{
	recursive_lock_lock(&mount->rlock);

	v->mount_next = mount->vnodes_head;
	v->mount_prev = NULL;
	mount->vnodes_head = v;
	if(!mount->vnodes_tail)
		mount->vnodes_tail = v;

	recursive_lock_unlock(&mount->rlock);
}

static void remove_vnode_from_mount_list(struct vnode *v, struct fs_mount *mount)
{
	recursive_lock_lock(&mount->rlock);

	if(v->mount_next)
		v->mount_next->mount_prev = v->mount_prev;
	else
		mount->vnodes_tail = v->mount_prev;
	if(v->mount_prev)
		v->mount_prev->mount_next = v->mount_next;
	else
		mount->vnodes_head = v->mount_next;

	v->mount_prev = v->mount_next = NULL;

	recursive_lock_unlock(&mount->rlock);
}

static struct vnode *create_new_vnode(void)
{
	struct vnode *v;

	v = (struct vnode *)kmalloc(sizeof(struct vnode));
	if(v == NULL)
		return NULL;

	init_vnode(v);
	return v;
}

static int dec_vnode_ref_count(struct vnode *v, bool free_mem, bool r)
{
	int err;
	int old_ref;

	mutex_lock(&vfs_vnode_mutex);

	if(v->busy == true)
		panic("dec_vnode_ref_count called on vnode that was busy! vnode 0x%x\n", v);

	old_ref = atomic_add(&v->ref_count, -1);
#if MAKE_NOIZE
	dprintf("dec_vnode_ref_count: vnode 0x%x, ref now %d\n", v, v->ref_count);
#endif

	if(old_ref == 1) {
		v->busy = true;

		mutex_unlock(&vfs_vnode_mutex);

		/* if we have a vm_cache attached, remove it */
		if(v->cache)
			vm_cache_release_ref((vm_cache_ref *)v->cache);
		v->cache = NULL;

		if(v->delete_me)
			v->mount->fs->calls->fs_removevnode(v->mount->fscookie, v->priv_vnode, r);
		else
			v->mount->fs->calls->fs_putvnode(v->mount->fscookie, v->priv_vnode, r);

		remove_vnode_from_mount_list(v, v->mount);

		mutex_lock(&vfs_vnode_mutex);
		hash_remove(vnode_table, v);
		mutex_unlock(&vfs_vnode_mutex);

		if(free_mem)
			kfree(v);
		err = 1;
	} else {
		mutex_unlock(&vfs_vnode_mutex);
		err = 0;
	}
	return err;
}

static int inc_vnode_ref_count(struct vnode *v)
{
	atomic_add(&v->ref_count, 1);
#if MAKE_NOIZE
	dprintf("inc_vnode_ref_count: vnode 0x%x, ref now %d\n", v, v->ref_count);
#endif
	return 0;
}

static struct vnode *lookup_vnode(fs_id fsid, vnode_id vnid)
{
	struct vnode_hash_key key;

	key.fsid = fsid;
	key.vnid = vnid;

	return hash_lookup(vnode_table, &key);
}

static int get_vnode(fs_id fsid, vnode_id vnid, struct vnode **outv, int r)
{
	struct vnode *v;
	int err;

#if MAKE_NOIZE
	dprintf("get_vnode: fsid %d vnid 0x%x 0x%x\n", fsid, vnid);
#endif

	mutex_lock(&vfs_vnode_mutex);

	do {
		v = lookup_vnode(fsid, vnid);
		if(v) {
			if(v->busy) {
				mutex_unlock(&vfs_vnode_mutex);
				thread_snooze(10000); // 10 ms
				mutex_lock(&vfs_vnode_mutex);
				continue;
			}
		}
	} while(0);

#if MAKE_NOIZE
	dprintf("get_vnode: tried to lookup vnode, got 0x%x\n", v);
#endif
	if(v) {
		inc_vnode_ref_count(v);
	} else {
		// we need to create a new vnode and read it in
		v = create_new_vnode();
		if(!v) {
			err = ERR_NO_MEMORY;
			goto err;
		}
		v->fsid = fsid;
		v->vnid = vnid;
		v->mount = fsid_to_mount(fsid);
		if(!v->mount) {
			err = ERR_INVALID_HANDLE;
			goto err;
		}
		v->busy = true;
		hash_insert(vnode_table, v);
		mutex_unlock(&vfs_vnode_mutex);

		add_vnode_to_mount_list(v, v->mount);

		err = v->mount->fs->calls->fs_getvnode(v->mount->fscookie, vnid, &v->priv_vnode, r);

		if(err < 0)
			remove_vnode_from_mount_list(v, v->mount);

		mutex_lock(&vfs_vnode_mutex);
		if(err < 0)
			goto err1;

		v->busy = false;
		v->ref_count = 1;
	}

	mutex_unlock(&vfs_vnode_mutex);
#if MAKE_NOIZE
	dprintf("get_vnode: returning 0x%x\n", v);
#endif
	*outv = v;
	return NO_ERROR;

err1:
	hash_remove(vnode_table, v);
err:
	mutex_unlock(&vfs_vnode_mutex);
	if(v)
		kfree(v);

	return err;
}

static void put_vnode(struct vnode *v)
{
	dec_vnode_ref_count(v, true, false);
}

int vfs_get_vnode(fs_id fsid, vnode_id vnid, fs_vnode *fsv)
{
	int err;
	struct vnode *v;

	err = get_vnode(fsid, vnid, &v, true);
	if(err < 0)
		return err;

	*fsv = v->priv_vnode;
	return NO_ERROR;
}

int vfs_put_vnode(fs_id fsid, vnode_id vnid)
{
	struct vnode *v;

	mutex_lock(&vfs_vnode_mutex);

	v = lookup_vnode(fsid, vnid);

	mutex_unlock(&vfs_vnode_mutex);
	if(v)
		dec_vnode_ref_count(v, true, true);

	return NO_ERROR;
}

void vfs_vnode_acquire_ref(void *v)
{
#if MAKE_NOIZE
	dprintf("vfs_vnode_acquire_ref: vnode 0x%x\n", v);
#endif
	inc_vnode_ref_count((struct vnode *)v);
}

void vfs_vnode_release_ref(void *v)
{
#if MAKE_NOIZE
	dprintf("vfs_vnode_release_ref: vnode 0x%x\n", v);
#endif
	dec_vnode_ref_count((struct vnode *)v, true, false);
}

int vfs_remove_vnode(fs_id fsid, vnode_id vnid)
{
	struct vnode *v;

	mutex_lock(&vfs_vnode_mutex);

	v = lookup_vnode(fsid, vnid);
	if(v)
		v->delete_me = true;

	mutex_unlock(&vfs_vnode_mutex);
	return 0;
}

void *vfs_get_cache_ptr(void *vnode)
{
	return ((struct vnode *)vnode)->cache;
}

int vfs_set_cache_ptr(void *vnode, void *cache)
{
	if(test_and_set((int *)&(((struct vnode *)vnode)->cache), (int)cache, 0) == 0)
		return -1;
	else
		return 0;
}

static struct file_descriptor *alloc_fd(void)
{
	struct file_descriptor *f;

	f = kmalloc(sizeof(struct file_descriptor));
	if(f) {
		f->vnode = NULL;
		f->cookie = NULL;
		f->ref_count = 1;
	}
	return f;
}

static struct file_descriptor *get_fd(struct ioctx *ioctx, int fd)
{
	struct file_descriptor *f;

	mutex_lock(&ioctx->io_mutex);

	if(fd >= 0 && fd < ioctx->table_size && ioctx->fds[fd]) {
		// valid fd
		f = ioctx->fds[fd];
		atomic_add(&f->ref_count, 1);
 	} else {
		f = NULL;
	}

	mutex_unlock(&ioctx->io_mutex);
	return f;
}

static void free_fd(struct file_descriptor *f)
{
	f->vnode->mount->fs->calls->fs_close(f->vnode->mount->fscookie, f->vnode->priv_vnode, f->cookie);
	f->vnode->mount->fs->calls->fs_freecookie(f->vnode->mount->fscookie, f->vnode->priv_vnode, f->cookie);
	dec_vnode_ref_count(f->vnode, true, false);
	kfree(f);
}

static void put_fd(struct file_descriptor *f)
{
	if(atomic_add(&f->ref_count, -1) == 1) {
		free_fd(f);
	}
}

static void remove_fd(struct ioctx *ioctx, int fd)
{
	struct file_descriptor *f;

	mutex_lock(&ioctx->io_mutex);

	if(fd >= 0 && fd < ioctx->table_size && ioctx->fds[fd]) {
		// valid fd
		f = ioctx->fds[fd];
		ioctx->fds[fd] = NULL;
		ioctx->num_used_fds--;
	} else {
		f = NULL;
	}

	mutex_unlock(&ioctx->io_mutex);

	if(f)
		put_fd(f);
}

static int new_fd(struct ioctx *ioctx, struct file_descriptor *f)
{
	int fd;
	int i;

	mutex_lock(&ioctx->io_mutex);

	fd = -1;
	for(i=0; i<ioctx->table_size; i++) {
		if(!ioctx->fds[i]) {
			fd = i;
			break;
		}
	}
	if(fd < 0)
		goto err;

	ioctx->fds[fd] = f;
	ioctx->num_used_fds++;

err:
	mutex_unlock(&ioctx->io_mutex);

	return fd;
}

static struct vnode *get_vnode_from_fd(struct ioctx *ioctx, int fd)
{
	struct file_descriptor *f;
	struct vnode *v;

	f = get_fd(ioctx, fd);
	if(!f) {
		return NULL;
	}

	v = f->vnode;
	if(!v)
		goto out;
	inc_vnode_ref_count(v);

out:
	put_fd(f);

	return v;
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

static int path_to_vnode(char *path, struct vnode **v, bool kernel)
{
	char *p = path;
	char *next_p;
	struct vnode *curr_v;
	struct vnode *next_v;
	vnode_id vnid;
	int err;

	if(!p)
		return ERR_INVALID_ARGS;

	// figure out if we need to start at root or at cwd
	if(*p == '/') {
		while(*++p == '/')
			;
		curr_v = root_vnode;
		inc_vnode_ref_count(curr_v);
	} else {
		struct ioctx *ioctx = get_current_ioctx(kernel);

		mutex_lock(&ioctx->io_mutex);
		curr_v = ioctx->cwd;
		inc_vnode_ref_count(curr_v);
		mutex_unlock(&ioctx->io_mutex);
	}

	for(;;) {
//		dprintf("path_to_vnode: top of loop. p 0x%x, *p = %c, p = '%s'\n", p, *p, p);

		// done?
		if(*p == '\0') {
			err = 0;
			break;
		}

		// walk to find the next component
		for(next_p = p+1; *next_p != '\0' && *next_p != '/'; next_p++);

		if(*next_p == '/') {
			*next_p = '\0';
			do
				next_p++;
			while(*next_p == '/');
		}

		// see if the .. is at the root of a mount
		if(strcmp("..", p) == 0 && curr_v->mount->root_vnode == curr_v) {
			// move to the covered vnode so we pass the '..' parse to the underlying filesystem
			if(curr_v->mount->covers_vnode) {
				next_v = curr_v->mount->covers_vnode;
				inc_vnode_ref_count(next_v);
				dec_vnode_ref_count(curr_v, false, false);
				curr_v = next_v;
			}
		}

		// tell the filesystem to parse this path
		err = curr_v->mount->fs->calls->fs_lookup(curr_v->mount->fscookie, curr_v->priv_vnode, p, &vnid);
		if(err < 0) {
			dec_vnode_ref_count(curr_v, false, false);
			goto out;
		}

		// lookup the vnode, the call to fs_lookup should have caused a get_vnode to be called
		// from inside the filesystem, thus the vnode would have to be in the list and it's
		// ref count incremented at this point
		mutex_lock(&vfs_vnode_mutex);
		next_v = lookup_vnode(curr_v->fsid, vnid);
		mutex_unlock(&vfs_vnode_mutex);

		if(!next_v) {
			// pretty screwed up here
			panic("path_to_vnode: could not lookup vnode (fsid 0x%x vnid 0x%x 0x%x)\n", curr_v->fsid, vnid);
			err = ERR_VFS_PATH_NOT_FOUND;
			dec_vnode_ref_count(curr_v, false, false);
			goto out;
		}

		// decrease the ref count on the old dir we just looked up into
		dec_vnode_ref_count(curr_v, false, false);

		p = next_p;
		curr_v = next_v;

		// see if we hit a mount point
		if(curr_v->covered_by) {
			next_v = curr_v->covered_by;
			inc_vnode_ref_count(next_v);
			dec_vnode_ref_count(curr_v, false, false);
			curr_v = next_v;
		}
	}

	*v = curr_v;

out:
	return err;
}

// returns the vnode in the next to last segment of the path, returns the last
// portion in filename
static int path_to_dir_vnode(char *path, struct vnode **v, char *filename, bool kernel)
{
	char *p;

	p = strrchr(path, '/');
	if(!p) {
		// this path is single segment with no '/' in it
		// ex. "foo"
		strcpy(filename, path);
		strcpy(path, ".");
	} else {
		// replace the filename portion of the path with a '.'
		strcpy(filename, p+1);
		p[1] = '.';
		p[2] = '\0';
	}
	return path_to_vnode(path, v, kernel);
}

#define NEW_FD_TABLE_SIZE 32
void *vfs_new_ioctx(void *_parent_ioctx)
{
	size_t table_size;
	struct ioctx *ioctx;
	struct ioctx *parent_ioctx;
	int i;

	parent_ioctx = (struct ioctx *)_parent_ioctx;
	if(parent_ioctx) {
		table_size = parent_ioctx->table_size;
	} else {
		table_size = NEW_FD_TABLE_SIZE;
	}

	ioctx = kmalloc(sizeof(struct ioctx) + sizeof(struct file_descriptor) * table_size);
	if(ioctx == NULL)
		return NULL;


	memset(ioctx, 0, sizeof(struct ioctx) + sizeof(struct file_descriptor) * table_size);

	if(mutex_init(&ioctx->io_mutex, "ioctx_mutex") < 0) {
		kfree(ioctx);
		return NULL;
	}

	/*
	 * copy parent files
	 */
	if(parent_ioctx) {
		size_t i;

		mutex_lock(&parent_ioctx->io_mutex);

		ioctx->cwd= parent_ioctx->cwd;
		if(ioctx->cwd) {
			inc_vnode_ref_count(ioctx->cwd);
		}
		

		for(i = 0; i< table_size; i++) {
			if(parent_ioctx->fds[i]) {
				ioctx->fds[i]= parent_ioctx->fds[i];
				atomic_add(&ioctx->fds[i]->ref_count, 1);
			}
		}

		mutex_unlock(&parent_ioctx->io_mutex);
	} else {
		ioctx->cwd = root_vnode;

		if(ioctx->cwd) {
			inc_vnode_ref_count(ioctx->cwd);
		}
	}

	ioctx->table_size = table_size;

	return ioctx;
}

int vfs_free_ioctx(void *_ioctx)
{
	struct ioctx *ioctx = (struct ioctx *)_ioctx;
	int i;

	if(ioctx->cwd)
		dec_vnode_ref_count(ioctx->cwd, true, false);

	mutex_lock(&ioctx->io_mutex);

	for(i=0; i<ioctx->table_size; i++) {
		if(ioctx->fds[i]) {
			put_fd(ioctx->fds[i]);
		}
	}

	mutex_unlock(&ioctx->io_mutex);

	mutex_destroy(&ioctx->io_mutex);

	kfree(ioctx);
	return 0;
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
	{
		struct fs_mount *mount;
		mounts_table = hash_init(MOUNTS_HASH_TABLE_SIZE, (addr)&mount->next - (addr)mount,
			&mount_compare, &mount_hash);
		if(mounts_table == NULL)
			panic("vfs_init: error creating mounts hash table\n");
	}
	fs_list = NULL;
	root_vnode = NULL;

	if(mutex_init(&vfs_mutex, "vfs_lock") < 0)
		panic("vfs_init: error allocating vfs lock\n");

	if(mutex_init(&vfs_mount_op_mutex, "vfs_mount_op_lock") < 0)
		panic("vfs_init: error allocating vfs_mount_op lock\n");

	if(mutex_init(&vfs_mount_mutex, "vfs_mount_lock") < 0)
		panic("vfs_init: error allocating vfs_mount lock\n");

	if(mutex_init(&vfs_vnode_mutex, "vfs_vnode_lock") < 0)
		panic("vfs_init: error allocating vfs_vnode lock\n");

	return 0;
}

int vfs_test(void)
{
	int fd;
	int err;

	dprintf("vfs_test() entry\n");

	fd = sys_open("/", STREAM_TYPE_DIR, 0);
	dprintf("fd = %d\n", fd);
	sys_close(fd);

	fd = sys_open("/", STREAM_TYPE_DIR, 0);
	dprintf("fd = %d\n", fd);

	sys_create("/foo", STREAM_TYPE_DIR);
	sys_create("/foo/bar", STREAM_TYPE_DIR);
	sys_create("/foo/bar/gar", STREAM_TYPE_DIR);
	sys_create("/foo/bar/tar", STREAM_TYPE_DIR);

#if 1
	fd = sys_open("/foo/bar", STREAM_TYPE_DIR, 0);
	if(fd < 0)
		panic("unable to open /foo/bar\n");

	{
		char buf[64];
		ssize_t len;

		sys_seek(fd, 0, SEEK_SET);
		for(;;) {
			len = sys_read(fd, buf, -1, sizeof(buf));
			if(len < 0)
				panic("readdir returned %d\n", len);
			if(len > 0)
				dprintf("readdir returned name = '%s'\n", buf);
			else
				break;
		}
	}

	// do some unlink tests
	err = sys_unlink("/foo/bar");
	if(err == NO_ERROR)
		panic("unlink of full directory should not have passed\n");
	sys_unlink("/foo/bar/gar");
	sys_unlink("/foo/bar/tar");
	err = sys_unlink("/foo/bar");
	if(err != NO_ERROR)
		panic("unlink of empty directory should have worked\n");

	sys_create("/test", STREAM_TYPE_DIR);
	sys_create("/test", STREAM_TYPE_DIR);
	err = sys_mount("/test", NULL, "rootfs", NULL);
	if(err < 0)
		panic("failed mount test\n");

#endif
#if 1

	fd = sys_open("/boot", STREAM_TYPE_DIR, 0);
	dprintf("fd = %d\n", fd);
	sys_close(fd);

	fd = sys_open("/boot", STREAM_TYPE_DIR, 0);
	if(fd < 0)
		panic("unable to open dir /boot\n");
	{
		char buf[64];
		ssize_t len;

		sys_seek(fd, 0, SEEK_SET);
		for(;;) {
			len = sys_read(fd, buf, -1, sizeof(buf));
			if(len < 0)
				panic("readdir returned %d\n", len);
			if(len > 0)
				dprintf("readdir returned name = '%s'\n", buf);
			else
				break;
		}
	}
	sys_close(fd);

	fd = sys_open("/boot/kernel", STREAM_TYPE_FILE, 0);
	if(fd < 0)
		panic("unable to open kernel file '/boot/kernel'\n");
	{
		char buf[64];
		ssize_t len;

		len = sys_read(fd, buf, 0, sizeof(buf));
		if(len < 0)
			panic("failed on read\n");
		dprintf("read returned %d\n", len);
	}
	sys_close(fd);
	{
		struct file_stat stat;

		err = sys_rstat("/boot/kernel", &stat);
		if(err < 0)
			panic("err stating '/boot/kernel'\n");
		dprintf("stat results:\n");
		dprintf("\tvnid 0x%x 0x%x\n\ttype %d\n\tsize 0x%x 0x%x\n", stat.vnid, stat.type, stat.size);
	}

#endif
	dprintf("vfs_test() done\n");
	panic("foo\n");
	return 0;
}


int vfs_register_filesystem(const char *name, struct fs_calls *calls)
{
	struct fs_container *container;

	container = (struct fs_container *)kmalloc(sizeof(struct fs_container));
	if(container == NULL)
		return ERR_NO_MEMORY;

	container->name = name;
	container->calls = calls;

	mutex_lock(&vfs_mutex);

	container->next = fs_list;
	fs_list = container;

	mutex_unlock(&vfs_mutex);
	return 0;
}

static int vfs_mount(char *path, const char *device, const char *fs_name, void *args, bool kernel)
{
	struct fs_mount *mount;
	int err = 0;
	struct vnode *covered_vnode = NULL;
	vnode_id root_id;

#if MAKE_NOIZE
	dprintf("vfs_mount: entry. path = '%s', fs_name = '%s'\n", path, fs_name);
#endif

	mutex_lock(&vfs_mount_op_mutex);

	mount = (struct fs_mount *)kmalloc(sizeof(struct fs_mount));
	if(mount == NULL)  {
		err = ERR_NO_MEMORY;
		goto err;
	}

	mount->mount_point = kstrdup(path);
	if(mount->mount_point == NULL) {
		err = ERR_NO_MEMORY;
		goto err1;
	}

	mount->fs = find_fs(fs_name);
	if(mount->fs == NULL) {
		err = ERR_VFS_INVALID_FS;
		goto err2;
	}

	recursive_lock_create(&mount->rlock);
	mount->id = next_fsid++;
	mount->unmounting = false;

	if(!root_vnode) {
		// we haven't mounted anything yet
		if(strcmp(path, "/") != 0) {
			err = ERR_VFS_GENERAL;
			goto err3;
		}

		err = mount->fs->calls->fs_mount(&mount->fscookie, mount->id, device, NULL, &root_id);
		if(err < 0) {
			err = ERR_VFS_GENERAL;
			goto err3;
		}

		mount->covers_vnode = NULL; // this is the root mount
	} else {
		int fd;
		struct ioctx *ioctx;
		void *null_cookie;

		err = path_to_vnode(path,&covered_vnode,kernel);
		if(err < 0) {
			goto err2;
		}

		if(!covered_vnode) {
			err = ERR_VFS_GENERAL;
			goto err2;
		}

		// XXX insert check to make sure covered_vnode is a DIR, or maybe it's okay for it not to be

		if((covered_vnode != root_vnode) && (covered_vnode->mount->root_vnode == covered_vnode)){
			err = ERR_VFS_ALREADY_MOUNTPOINT;
			goto err2;
		}

		mount->covers_vnode = covered_vnode;

		// mount it
		err = mount->fs->calls->fs_mount(&mount->fscookie, mount->id, device, NULL, &root_id);
		if(err < 0)
			goto err4;
	}

	mutex_lock(&vfs_mount_mutex);

	// insert mount struct into list
	hash_insert(mounts_table, mount);

	mutex_unlock(&vfs_mount_mutex);

	err = get_vnode(mount->id, root_id, &mount->root_vnode, 0);
	if(err < 0)
		goto err5;

	// XXX may be a race here
	if(mount->covers_vnode)
		mount->covers_vnode->covered_by = mount->root_vnode;

	if(!root_vnode)
		root_vnode = mount->root_vnode;

	mutex_unlock(&vfs_mount_op_mutex);

	return 0;

err5:
	mount->fs->calls->fs_unmount(mount->fscookie);
err4:
	if(mount->covers_vnode)
		dec_vnode_ref_count(mount->covers_vnode, true, false);
err3:
	recursive_lock_destroy(&mount->rlock);
err2:
	kfree(mount->mount_point);
err1:
	kfree(mount);
err:
	mutex_unlock(&vfs_mount_op_mutex);

	return err;
}

static int vfs_unmount(char *path, bool kernel)
{
	struct vnode *v;
	struct fs_mount *mount;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_unmount: entry. path = '%s', kernel %d\n", path, kernel);
#endif

	err = path_to_vnode(path, &v, kernel);
	if(err < 0) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	mutex_lock(&vfs_mount_op_mutex);

	mount = fsid_to_mount(v->fsid);
	if(!mount) {
		panic("vfs_unmount: fsid_to_mount failed on root vnode @0x%x of mount\n", v);
	}

	if(mount->root_vnode != v) {
		// not mountpoint
		dec_vnode_ref_count(v, true, false);
		err = ERR_VFS_NOT_MOUNTPOINT;
		goto err1;
	}

	/* grab the vnode master mutex to keep someone from creating a vnode
	while we're figuring out if we can continue */
	mutex_lock(&vfs_vnode_mutex);

	/* simulate the root vnode having it's refcount decremented */
	mount->root_vnode->ref_count -= 2;

	/* cycle through the list of vnodes associated with this mount and
	make sure all of them are not busy or have refs on them */
	err = 0;
	for(v = mount->vnodes_head; v; v = v->mount_next) {
		if(v->busy || v->ref_count != 0) {
			mount->root_vnode->ref_count += 2;
			mutex_unlock(&vfs_vnode_mutex);
			dec_vnode_ref_count(mount->root_vnode, true, false);
			err = ERR_VFS_FS_BUSY;
			goto err1;
		}
	}

	/* we can safely continue, mark all of the vnodes busy and this mount
	structure in unmounting state */
	for(v = mount->vnodes_head; v; v = v->mount_next)
		if(v != mount->root_vnode)
			v->busy = true;
	mount->unmounting = true;

	mutex_unlock(&vfs_vnode_mutex);

	mount->covers_vnode->covered_by = NULL;
	dec_vnode_ref_count(mount->covers_vnode, true, false);

	/* release the ref on the root vnode twice */
	dec_vnode_ref_count(mount->root_vnode, true, false);
	dec_vnode_ref_count(mount->root_vnode, true, false);

	// XXX when full vnode cache in place, will need to force
	// a putvnode/removevnode here

	/* remove the mount structure from the hash table */
	mutex_lock(&vfs_mount_mutex);
	hash_remove(mounts_table, mount);
	mutex_unlock(&vfs_mount_mutex);

	mutex_unlock(&vfs_mount_op_mutex);

	mount->fs->calls->fs_unmount(mount->fscookie);

	kfree(mount->mount_point);
	kfree(mount);

	return 0;

err1:
	mutex_unlock(&vfs_mount_op_mutex);
err:
	return err;
}

static int vfs_sync(void)
{
	struct hash_iterator iter;
	struct fs_mount *mount;

#if MAKE_NOIZE
	dprintf("vfs_sync: entry.\n");
#endif

	/* cycle through and call sync on each mounted fs */
	mutex_lock(&vfs_mount_op_mutex);
	mutex_lock(&vfs_mount_mutex);

	hash_open(mounts_table, &iter);
	while((mount = hash_next(mounts_table, &iter))) {
		mount->fs->calls->fs_sync(mount->fscookie);
	}
	hash_close(mounts_table, &iter, false);

	mutex_unlock(&vfs_mount_mutex);
	mutex_unlock(&vfs_mount_op_mutex);

	return 0;
}

static int vfs_open(char *path, stream_type st, int omode, bool kernel)
{
	int fd;
	struct vnode *v;
	file_cookie cookie;
	struct file_descriptor *f;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_open: entry. path = '%s', omode %d, kernel %d\n", path, omode, kernel);
#endif

	err = path_to_vnode(path, &v, kernel);
	if(err < 0)
		goto err;

	err = v->mount->fs->calls->fs_open(v->mount->fscookie, v->priv_vnode, &cookie, st, omode);
	if(err < 0)
		goto err1;

	// file is opened, create a fd
	f = alloc_fd();
	if(!f) {
		// xxx leaks
		err = ERR_NO_MEMORY;
		goto err1;
	}
	f->vnode = v;
	f->cookie = cookie;

	fd = new_fd(get_current_ioctx(kernel), f);
	if(fd < 0) {
		err = ERR_VFS_FD_TABLE_FULL;
		goto err1;
	}

	return fd;

err2:
	free_fd(f);
err1:
	dec_vnode_ref_count(v, true, false);
err:
	return err;
}

static int vfs_close(int fd, bool kernel)
{
	struct file_descriptor *f;
	struct ioctx *ioctx;

#if MAKE_NOIZE
	dprintf("vfs_close: entry. fd %d, kernel %d\n", fd, kernel);
#endif

	ioctx = get_current_ioctx(kernel);

	f = get_fd(ioctx, fd);
	if(!f)
		return ERR_INVALID_HANDLE;

	remove_fd(ioctx, fd);
	put_fd(f);

	return 0;
}

static int vfs_fsync(int fd, bool kernel)
{
	struct file_descriptor *f;
	struct vnode *v;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_fsync: entry. fd %d kernel %d\n", fd, kernel);
#endif

	f = get_fd(get_current_ioctx(kernel), fd);
	if(!f)
		return ERR_INVALID_HANDLE;

	v = f->vnode;
	err = v->mount->fs->calls->fs_fsync(v->mount->fscookie, v->priv_vnode);

	put_fd(f);

	return err;
}

static ssize_t vfs_read(int fd, void *buf, off_t pos, ssize_t len, bool kernel)
{
	struct vnode *v;
	struct file_descriptor *f;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_read: fd = %d, buf 0x%x, pos 0x%x 0x%x, len 0x%x, kernel %d\n", fd, buf, pos, len, kernel);
#endif

	f = get_fd(get_current_ioctx(kernel), fd);
	if(!f) {
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	v = f->vnode;
	err = v->mount->fs->calls->fs_read(v->mount->fscookie, v->priv_vnode, f->cookie, buf, pos, len);

	put_fd(f);

err:
	return err;
}

static ssize_t vfs_write(int fd, const void *buf, off_t pos, ssize_t len, bool kernel)
{
	struct vnode *v;
	struct file_descriptor *f;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_write: fd = %d, buf 0x%x, pos 0x%x 0x%x, len 0x%x, kernel %d\n", fd, buf, pos, len, kernel);
#endif

	f = get_fd(get_current_ioctx(kernel), fd);
	if(!f) {
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	v = f->vnode;
	err = v->mount->fs->calls->fs_write(v->mount->fscookie, v->priv_vnode, f->cookie, buf, pos, len);

	put_fd(f);

err:
	return err;
}

static int vfs_seek(int fd, off_t pos, seek_type seek_type, bool kernel)
{
	struct vnode *v;
	struct file_descriptor *f;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_seek: fd = %d, pos 0x%x 0x%x, seek_type %d, kernel %d\n", fd, pos, seek_type, kernel);
#endif

	f = get_fd(get_current_ioctx(kernel), fd);
	if(!f) {
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	v = f->vnode;
	err = v->mount->fs->calls->fs_seek(v->mount->fscookie, v->priv_vnode, f->cookie, pos, seek_type);

	put_fd(f);

err:
	return err;

}

static int vfs_ioctl(int fd, int op, void *buf, size_t len, bool kernel)
{
	struct vnode *v;
	struct file_descriptor *f;
	int err;

#if MAKE_NOIZE
	dprintf("vfs_ioctl: fd = %d, op 0x%x, buf 0x%x, len 0x%x, kernel %d\n", fd, op, buf, len, kernel);
#endif

	f = get_fd(get_current_ioctx(kernel), fd);
	if(!f) {
		err = ERR_INVALID_HANDLE;
		goto err;
	}

	v = f->vnode;
	err = v->mount->fs->calls->fs_ioctl(v->mount->fscookie, v->priv_vnode, f->cookie, op, buf, len);

	put_fd(f);

err:
	return err;
}

static int vfs_create(char *path, stream_type stream_type, void *args, bool kernel)
{
	int err;
	struct vnode *v;
	char filename[SYS_MAX_NAME_LEN];
	vnode_id vnid;

#if MAKE_NOIZE
	dprintf("vfs_create: path '%s', stream_type %d, args 0x%x, kernel %d\n", path, stream_type, args, kernel);
#endif

	err = path_to_dir_vnode(path, &v, filename, kernel);
	if(err < 0)
		goto err;

	err = v->mount->fs->calls->fs_create(v->mount->fscookie, v->priv_vnode, filename, stream_type, args, &vnid);

err1:
	dec_vnode_ref_count(v, true, false);
err:
	return err;
}

static int vfs_unlink(char *path, bool kernel)
{
	int err;
	struct vnode *v;
	char filename[SYS_MAX_NAME_LEN];

#if MAKE_NOIZE
	dprintf("vfs_unlink: path '%s', kernel %d\n", path, kernel);
#endif

	err = path_to_dir_vnode(path, &v, filename, kernel);
	if(err < 0)
		goto err;

	err = v->mount->fs->calls->fs_unlink(v->mount->fscookie, v->priv_vnode, filename);

err1:
	dec_vnode_ref_count(v, true, false);
err:
	return err;
}

static int vfs_rename(char *path, char *newpath, bool kernel)
{
	struct vnode *v1, *v2;
	char filename1[SYS_MAX_NAME_LEN];
	char filename2[SYS_MAX_NAME_LEN];
	int err;

	err = path_to_dir_vnode(path, &v1, filename1, kernel);
	if(err < 0)
		goto err;

	err = path_to_dir_vnode(path, &v2, filename2, kernel);
	if(err < 0)
		goto err1;

	if(v1->fsid != v2->fsid) {
		err = ERR_VFS_CROSS_FS_RENAME;
		goto err2;
	}

	err = v1->mount->fs->calls->fs_rename(v1->mount->fscookie, v1->priv_vnode, filename1, v2->priv_vnode, filename2);

err2:
	dec_vnode_ref_count(v2, true, false);
err1:
	dec_vnode_ref_count(v1, true, false);
err:
	return err;
}

static int vfs_rstat(char *path, struct file_stat *stat, bool kernel)
{
	int err;
	struct vnode *v;

#if MAKE_NOIZE
	dprintf("vfs_rstat: path '%s', stat 0x%x, kernel %d\n", path, stat, kernel);
#endif

	err = path_to_vnode(path, &v, kernel);
	if(err < 0)
		goto err;

	err = v->mount->fs->calls->fs_rstat(v->mount->fscookie, v->priv_vnode, stat);

err1:
	dec_vnode_ref_count(v, true, false);
err:
	return err;
}

static int vfs_wstat(char *path, struct file_stat *stat, int stat_mask, bool kernel)
{
	int err;
	struct vnode *v;

#if MAKE_NOIZE
	dprintf("vfs_wstat: path '%s', stat 0x%x, stat_mask %d, kernel %d\n", path, stat, stat_mask, kernel);
#endif

	err = path_to_vnode(path, &v, kernel);
	if(err < 0)
		goto err;

	err = v->mount->fs->calls->fs_wstat(v->mount->fscookie, v->priv_vnode, stat, stat_mask);

err1:
	dec_vnode_ref_count(v, true, false);
err:
	return err;
}

int vfs_get_vnode_from_fd(int fd, bool kernel, void **vnode)
{
	struct ioctx *ioctx;
	int err;

	ioctx = get_current_ioctx(kernel);
	*vnode = get_vnode_from_fd(ioctx, fd);

	if(*vnode == NULL)
		return ERR_INVALID_HANDLE;

	return NO_ERROR;
}

int vfs_get_vnode_from_path(const char *path, bool kernel, void **vnode)
{
	struct vnode *v;
	int err;
	char buf[SYS_MAX_PATH_LEN+1];

#if MAKE_NOIZE
	dprintf("vfs_get_vnode_from_path: entry. path = '%s', kernel %d\n", path, kernel);
#endif

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	err = path_to_vnode(buf, &v, kernel);
	if(err < 0)
		goto err;

	*vnode = v;

err:
	return err;
}

int vfs_put_vnode_ptr(void *vnode)
{
	struct vnode *v = vnode;

	put_vnode(v);

	return 0;
}

ssize_t vfs_canpage(void *_v)
{
	struct vnode *v = _v;

#if MAKE_NOIZE
	dprintf("vfs_canpage: vnode 0x%x\n", v);
#endif

	return v->mount->fs->calls->fs_canpage(v->mount->fscookie, v->priv_vnode);
}

ssize_t vfs_readpage(void *_v, iovecs *vecs, off_t pos)
{
	struct vnode *v = _v;

#if MAKE_NOIZE
	dprintf("vfs_readpage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos);
#endif

	return v->mount->fs->calls->fs_readpage(v->mount->fscookie, v->priv_vnode, vecs, pos);
}

ssize_t vfs_writepage(void *_v, iovecs *vecs, off_t pos)
{
	struct vnode *v = _v;

#if MAKE_NOIZE
	dprintf("vfs_writepage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos);
#endif

	return v->mount->fs->calls->fs_writepage(v->mount->fscookie, v->priv_vnode, vecs, pos);
}

static int vfs_get_cwd(char* buf, size_t size, bool kernel)
{
	// Get current working directory from io context
	struct vnode* v = get_current_ioctx(kernel)->cwd;
	int rc;

#if MAKE_NOIZE
	dprintf("vfs_get_cwd: buf 0x%x, 0x%x\n", buf, size);
#endif
	//TODO: parse cwd back into a full path
	if (size >= 2) {
		buf[0] = '.';
		buf[1] = 0;

		rc = 0;
	} else {
		rc = ERR_VFS_INSUFFICIENT_BUF;
	}

	// Tell caller all is ok
	return rc;
}

static int vfs_set_cwd(char* path, bool kernel)
{
	struct ioctx* curr_ioctx;
	struct vnode* v = NULL;
	struct vnode* old_cwd;
	struct file_stat stat;
	int rc;

#if MAKE_NOIZE
	dprintf("vfs_set_cwd: path=\'%s\'\n", path);
#endif

	// Get vnode for passed path, and bail if it failed
	rc = path_to_vnode(path, &v, kernel);
	if (rc < 0) {
		goto err;
	}

	rc = v->mount->fs->calls->fs_rstat(v->mount->fscookie, v->priv_vnode, &stat);
	if(rc < 0) {
		goto err1;
	}

	if(stat.type != STREAM_TYPE_DIR) {
		// nope, can't cwd to here
		rc = ERR_VFS_WRONG_STREAM_TYPE;
		goto err1;
	}

	// Get current io context and lock
	curr_ioctx = get_current_ioctx(kernel);
	mutex_lock(&curr_ioctx->io_mutex);

	// save the old cwd
	old_cwd = curr_ioctx->cwd;

	// Set the new vnode
	curr_ioctx->cwd = v;

	// Unlock the ioctx
	mutex_unlock(&curr_ioctx->io_mutex);

	// Decrease ref count of previous working dir (as the ref is being replaced)
	if (old_cwd)
		dec_vnode_ref_count(old_cwd, true, false);

	return NO_ERROR;

err1:
	dec_vnode_ref_count(v, true, false);
err:
	return rc;
}

int sys_mount(const char *path, const char *device, const char *fs_name, void *args)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_mount(buf, device, fs_name, args, true);
}

int sys_unmount(const char *path)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_unmount(buf, true);
}

int sys_sync(void)
{
	return vfs_sync();
}

int sys_open(const char *path, stream_type st, int omode)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_open(buf, st, omode, true);
}

int sys_close(int fd)
{
	return vfs_close(fd, true);
}

int sys_fsync(int fd)
{
	return vfs_fsync(fd, true);
}

ssize_t sys_read(int fd, void *buf, off_t pos, ssize_t len)
{
	return vfs_read(fd, buf, pos, len, true);
}

ssize_t sys_write(int fd, const void *buf, off_t pos, ssize_t len)
{
	return vfs_write(fd, buf, pos, len, true);
}

int sys_seek(int fd, off_t pos, seek_type seek_type)
{
	return vfs_seek(fd, pos, seek_type, true);
}

int sys_ioctl(int fd, int op, void *buf, size_t len)
{
	return vfs_ioctl(fd, op, buf, len, true);
}

int sys_create(const char *path, stream_type stream_type)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_create(buf, stream_type, NULL, true);
}

int sys_unlink(const char *path)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_unlink(buf, true);
}

int sys_rename(const char *oldpath, const char *newpath)
{
	char buf1[SYS_MAX_PATH_LEN+1];
	char buf2[SYS_MAX_PATH_LEN+1];

	strncpy(buf1, oldpath, SYS_MAX_PATH_LEN);
	buf1[SYS_MAX_PATH_LEN] = 0;

	strncpy(buf2, newpath, SYS_MAX_PATH_LEN);
	buf2[SYS_MAX_PATH_LEN] = 0;

	return vfs_rename(buf1, buf2, true);
}

int sys_rstat(const char *path, struct file_stat *stat)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_rstat(buf, stat, true);
}

int sys_wstat(const char *path, struct file_stat *stat, int stat_mask)
{
	char buf[SYS_MAX_PATH_LEN+1];

	strncpy(buf, path, SYS_MAX_PATH_LEN);
	buf[SYS_MAX_PATH_LEN] = 0;

	return vfs_wstat(buf, stat, stat_mask, true);
}

char* sys_getcwd(char *buf, size_t size)
{
	char path[SYS_MAX_PATH_LEN];
	int rc;

#if MAKE_NOIZE
	dprintf("sys_getcwd: buf 0x%x, 0x%x\n", buf, size);
#endif

	// Call vfs to get current working directory
	rc = vfs_get_cwd(path,SYS_MAX_PATH_LEN-1,true);
	path[SYS_MAX_PATH_LEN-1] = 0;

	// Copy back the result
	strncpy(buf,path,size);

	// Return either NULL or the buffer address to indicate failure or success
	return  (rc < 0) ? NULL : buf;
}

int sys_setcwd(const char* _path)
{
	char path[SYS_MAX_PATH_LEN];

#if MAKE_NOIZE
	dprintf("sys_setcwd: path=0x%x\n", _path);
#endif

	// Copy new path to kernel space
	strncpy(path, _path, SYS_MAX_PATH_LEN-1);
	path[SYS_MAX_PATH_LEN-1] = 0;

	// Call vfs to set new working directory
	return vfs_set_cwd(path,true);
}

int user_mount(const char *upath, const char *udevice, const char *ufs_name, void *args)
{
	char path[SYS_MAX_PATH_LEN+1];
	char fs_name[SYS_MAX_OS_NAME_LEN+1];
	char device[SYS_MAX_PATH_LEN+1];
	int rc;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	if((addr)ufs_name >= KERNEL_BASE && (addr)ufs_name <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	if(udevice) {
		if((addr)udevice >= KERNEL_BASE && (addr)udevice <= KERNEL_TOP)
			return ERR_VM_BAD_USER_MEMORY;
	}

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN] = 0;

	rc = user_strncpy(fs_name, ufs_name, SYS_MAX_OS_NAME_LEN);
	if(rc < 0)
		return rc;
	fs_name[SYS_MAX_OS_NAME_LEN] = 0;

	if(udevice) {
		rc = user_strncpy(device, udevice, SYS_MAX_PATH_LEN);
		if(rc < 0)
			return rc;
		device[SYS_MAX_PATH_LEN] = 0;
	} else {
		device[0] = 0;
	}

	return vfs_mount(path, device, fs_name, args, false);
}

int user_unmount(const char *upath)
{
	char path[SYS_MAX_PATH_LEN+1];
	int rc;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN] = 0;

	return vfs_unmount(path, false);
}

int user_sync(void)
{
	return vfs_sync();
}

int user_open(const char *upath, stream_type st, int omode)
{
	char path[SYS_MAX_PATH_LEN];
	int rc;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN-1);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN-1] = 0;

	return vfs_open(path, st, omode, false);
}

int user_close(int fd)
{
	return vfs_close(fd, false);
}

int user_fsync(int fd)
{
	return vfs_fsync(fd, false);
}

ssize_t user_read(int fd, void *buf, off_t pos, ssize_t len)
{
	if((addr)buf >= KERNEL_BASE && (addr)buf <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	return vfs_read(fd, buf, pos, len, false);
}

ssize_t user_write(int fd, const void *buf, off_t pos, ssize_t len)
{
	if((addr)buf >= KERNEL_BASE && (addr)buf <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	return vfs_write(fd, buf, pos, len, false);
}

int user_seek(int fd, off_t pos, seek_type seek_type)
{
	return vfs_seek(fd, pos, seek_type, false);
}

int user_ioctl(int fd, int op, void *buf, size_t len)
{
	if((addr)buf >= KERNEL_BASE && (addr)buf <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;
	return vfs_ioctl(fd, op, buf, len, false);
}

int user_create(const char *upath, stream_type stream_type)
{
	char path[SYS_MAX_PATH_LEN];
	int rc;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN-1);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN-1] = 0;

	return vfs_create(path, stream_type, NULL, false);
}

int user_unlink(const char *upath)
{
	char path[SYS_MAX_PATH_LEN+1];
	int rc;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN] = 0;

	return vfs_unlink(path, false);
}

int user_rename(const char *uoldpath, const char *unewpath)
{
	char oldpath[SYS_MAX_PATH_LEN+1];
	char newpath[SYS_MAX_PATH_LEN+1];
	int rc;

	if((addr)uoldpath >= KERNEL_BASE && (addr)uoldpath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	if((addr)unewpath >= KERNEL_BASE && (addr)unewpath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(oldpath, uoldpath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	oldpath[SYS_MAX_PATH_LEN] = 0;

	rc = user_strncpy(newpath, unewpath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	newpath[SYS_MAX_PATH_LEN] = 0;

	return vfs_rename(oldpath, newpath, false);
}

int user_rstat(const char *upath, struct file_stat *ustat)
{
	char path[SYS_MAX_PATH_LEN];
	struct file_stat stat;
	int rc, rc2;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	if((addr)ustat >= KERNEL_BASE && (addr)ustat <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN-1);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN-1] = 0;

	rc = vfs_rstat(path, &stat, false);
	if(rc < 0)
		return rc;

	rc2 = user_memcpy(ustat, &stat, sizeof(struct file_stat));
	if(rc2 < 0)
		return rc2;

	return rc;
}

int user_wstat(const char *upath, struct file_stat *ustat, int stat_mask)
{
	char path[SYS_MAX_PATH_LEN+1];
	struct file_stat stat;
	int rc;

	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	if((addr)ustat >= KERNEL_BASE && (addr)ustat <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN);
	if(rc < 0)
		return rc;
	path[SYS_MAX_PATH_LEN] = 0;

	rc = user_memcpy(&stat, ustat, sizeof(struct file_stat));
	if(rc < 0)
		return rc;

	return vfs_wstat(path, &stat, stat_mask, false);
}

int user_getcwd(char *buf, size_t size)
{
	char path[SYS_MAX_PATH_LEN];
	int rc, rc2;

#if MAKE_NOIZE
	dprintf("user_getcwd: buf 0x%x, 0x%x\n", buf, size);
#endif

	// Check if userspace address is inside "shared" kernel space
	if((addr)buf >= KERNEL_BASE && (addr)buf <= KERNEL_TOP)
		return NULL; //ERR_VM_BAD_USER_MEMORY;

	// Call vfs to get current working directory
	rc = vfs_get_cwd(path, SYS_MAX_PATH_LEN-1, false);
	if(rc < 0)
		return rc;

	// Copy back the result
	rc2 = user_strncpy(buf, path, size);
	if(rc2 < 0)
		return rc2;

	return rc;
}

int user_setcwd(const char* upath)
{
	char path[SYS_MAX_PATH_LEN];
	int rc;

#if MAKE_NOIZE
	dprintf("user_setcwd: path=0x%x\n", upath);
#endif

	// Check if userspace address is inside "shared" kernel space
	if((addr)upath >= KERNEL_BASE && (addr)upath <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	// Copy new path to kernel space
	rc = user_strncpy(path, upath, SYS_MAX_PATH_LEN-1);
	if (rc < 0) {
		return rc;
	}
	path[SYS_MAX_PATH_LEN-1] = 0;

	// Call vfs to set new working directory
	return vfs_set_cwd(path,false);
}

image_id vfs_load_fs_module(const char *name)
{
	image_id id;
	void (*bootstrap)();
	char path[SYS_MAX_PATH_LEN];

	sprintf(path, "/boot/addons/fs/%s", name);

	id = elf_load_kspace(path, "");
	if(id < 0)
		return id;

	bootstrap = (void *)elf_lookup_symbol(id, "fs_bootstrap");
	if(!bootstrap)
		return ERR_VFS_INVALID_FS;

	bootstrap();

	return id;
}

int vfs_bootstrap_all_filesystems(void)
{
	int err;
	int fd;

	// bootstrap the root filesystem
	bootstrap_rootfs();

	err = sys_mount("/", NULL, "rootfs", NULL);
	if(err < 0)
		panic("error mounting rootfs!\n");

	sys_setcwd("/");

	// bootstrap the bootfs
	bootstrap_bootfs();

	sys_create("/boot", STREAM_TYPE_DIR);
	err = sys_mount("/boot", NULL, "bootfs", NULL);
	if(err < 0)
		panic("error mounting bootfs\n");

	// bootstrap the devfs
	bootstrap_devfs();

	sys_create("/dev", STREAM_TYPE_DIR);
	err = sys_mount("/dev", NULL, "devfs", NULL);
	if(err < 0)
		panic("error mounting devfs\n");


	fd = sys_open("/boot/addons/fs", STREAM_TYPE_DIR, 0);
	if(fd >= 0) {
		ssize_t len;
		char buf[SYS_MAX_NAME_LEN];

		while((len = sys_read(fd, buf, 0, sizeof(buf))) > 0) {
			dprintf("loading '%s' fs module\n", buf);
			vfs_load_fs_module(buf);
		}
		sys_close(fd);
	}

	return NO_ERROR;
}

