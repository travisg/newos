#include <string.h>

#include <kernel.h>
#include <stage2.h>
#include <vfs.h>
#include <debug.h>
#include <khash.h>
#include <sem.h>
#include <arch_cpu.h>

#include <fs/rootfs.h>

#define MAKE_NOIZE 1

struct vnode {
	struct vnode *next;
	vnode_id id;
	void *priv_vnode;
	struct fs_mount *mount;
	int ref_count;
};

struct file_descriptor {
	struct vnode *vnode;
	int cookie_type;
	void *cookie;
	off_t pos;
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

static int vnode_hash(void *_v, void *key, int range)
{
	struct vnode *v = _v;
	
	if(v != NULL)
		return (((int)v->priv_vnode >> 3) % range);
	else
		return (((int)key >> 3) % range);
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
	int cookie_type = -1;
	int err = 0;

	if(!locked)
		sem_acquire(ioctx->io_sem, 1);

	ioctx->fds[fd].ref_count--;
	if(ioctx->fds[fd].ref_count == 0) {
		cookie = ioctx->fds[fd].cookie;
		cookie_type = ioctx->fds[fd].cookie_type;
		v = ioctx->fds[fd].vnode;
	
		ioctx->fds[fd].vnode = NULL;
		ioctx->fds[fd].cookie = NULL;
	}

	if(!locked)
		sem_release(ioctx->io_sem, 1);
		
	if(cookie != NULL) {
		// we need to free it
		switch(cookie_type) {
			case FILE_TYPE_DIR:
				err = v->mount->fs->calls->fs_closedir(v->mount->cookie, v);
				v->mount->fs->calls->fs_freedircookie(v->mount->cookie, cookie);
				dec_vnode_ref_count(v, false, true);
				break;
			case FILE_TYPE_FILE:
				// XXX finish here
//				v->mount->fs->calls->fs_freecookie(v->mount->cookie, cookie);
				break;
			default:
				dprintf("dec_fd_ref_count: unknown cookie type!\n"); 
		}
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
	TOUCH(ka);

	dprintf("vfs_init: entry\n");

	{
		struct vnode *v;
		vnode_table = hash_init(VNODE_HASH_TABLE_SIZE, (int)&v->next - (int)&v,
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

	// bootstrap a few filesystems
	bootstrap_rootfs();	

	vfs_mount("/", "rootfs");

	return 0;
}

int vfs_test()
{
	int fd;
	int err;
	
	fd = vfs_opendir(NULL, "/");
	dprintf("fd = %d\n", fd);
	vfs_closedir(fd);

	fd = vfs_opendir(NULL, "/");
	dprintf("fd = %d\n", fd);

	vfs_mkdir(NULL, "/boot");
	vfs_mkdir(NULL, "/boot");

	err = vfs_mount("/boot", "rootfs");
	if(err < 0)
		panic("failed mount test\n");

	fd = vfs_opendir(NULL, "/boot");
	dprintf("fd = %d\n", fd);
	vfs_closedir(fd);
	
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

static struct ioctx *get_current_ioctx()
{
	struct ioctx *ioctx;
	bool kernel = true; // XXX figure this out later

	if(kernel) {
		ioctx = proc_get_kernel_proc()->ioctx;
	} else {
		ioctx = thread_get_current_thread()->proc->ioctx;
	}
	return ioctx;
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

int vfs_mount(const char *path, const char *fs_name)
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
	} else {
		int fd;
		struct ioctx *ioctx;
		
		fd = vfs_opendir(NULL, path);
		if(fd < 0) {
			err = fd;
			goto err1;
		}
		// get the vnode this mount will cover
		ioctx = get_current_ioctx();
		covered_vnode = ioctx->fds[fd].vnode;
		inc_vnode_ref_count(covered_vnode, false);
		
		vfs_closedir(fd);

		// mount it
		err = mount->fs->calls->fs_mount(&mount->cookie, 0, covered_vnode, mount->id, &mount->root_vnode.priv_vnode);
		if(err < 0)
			goto err1;		
	}

	root_vnode = &mount->root_vnode;
	
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

int vfs_unmount(const char *path)
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

int vfs_opendir_loopback(void *_base_vnode, const char *path, void **vnode, void **dircookie)
{
	struct vnode *base_vnode = _base_vnode;
	
#if MAKE_NOIZE	
	dprintf("vfs_opendir_loopback: entry. path = '%s', base_vnode 0x%x\n", path, base_vnode);
#endif	
	return base_vnode->mount->fs->calls->fs_opendir(base_vnode->mount->cookie, base_vnode->priv_vnode, path, vnode, dircookie);
}

int vfs_opendir(void *_base_vnode, const char *path)
{
	int fd;
	struct ioctx *ioctx;
	struct vnode *base_vnode = _base_vnode;
	struct vnode *new_vnode;
	void *v;
	void *cookie;
	int err;
	
#if MAKE_NOIZE	
	dprintf("vfs_opendir: entry. path = '%s', base_vnode 0x%x\n", path, base_vnode);
#endif	
	ioctx = get_current_ioctx();

	if(base_vnode == NULL) {
		sem_acquire(ioctx->io_sem, 1);
	
		base_vnode = ioctx->cwd;
		inc_vnode_ref_count(base_vnode, false);
	
		sem_release(ioctx->io_sem, 1);
	}
	
	err = base_vnode->mount->fs->calls->fs_opendir(base_vnode->mount->cookie, base_vnode->priv_vnode, path, &v, &cookie);	
	if(err < 0)
		goto err;
	
	sem_acquire(vfs_vnode_sem, 1);

	new_vnode = hash_lookup(vnode_table, v);
	if(new_vnode == NULL) {
		// it wasn't in the list
		new_vnode = create_new_vnode();
		if(new_vnode == NULL) {
			sem_release(vfs_vnode_sem, 1);
			err = -1;
			goto err1;
		}
		new_vnode->priv_vnode = v;
		new_vnode->mount = base_vnode->mount;

		hash_insert(vnode_table, new_vnode);
	}
	inc_vnode_ref_count(new_vnode, true);
	dec_vnode_ref_count(base_vnode, true, true);

	sem_release(vfs_vnode_sem, 1);

	sem_acquire(ioctx->io_sem, 1);

	fd = get_new_fd(ioctx);
	if(fd < 0) {
		err = -1;
		goto err2;
	}

	ioctx->fds[fd].vnode = new_vnode;
	ioctx->fds[fd].cookie = cookie;
	ioctx->fds[fd].cookie_type = FILE_TYPE_DIR;
	ioctx->fds[fd].ref_count = 0;
	inc_fd_ref_count(ioctx, fd, true);

	sem_release(ioctx->io_sem, 1);

	return fd;

err2:
	// XXX remove the vnode if recently created
err1:
	// XXX insert fs_closedir here
err:		
	return err;
}

int vfs_closedir(int fd)
{
	struct ioctx *ioctx;

	ioctx = get_current_ioctx();
#if MAKE_NOIZE
	dprintf("vfs_closedir: fd = %d, ioctx = 0x%x\n", fd, ioctx);
#endif
	return dec_fd_ref_count(ioctx, fd, false);
}

int vfs_mkdir(void *_base_vnode, const char *path)
{
	struct vnode *base_vnode = _base_vnode;
	int err;

	if(base_vnode == NULL) {
		struct ioctx *ioctx;
			
		ioctx = get_current_ioctx();
		sem_acquire(ioctx->io_sem, 1);
	
		base_vnode = ioctx->cwd;
	
		sem_release(ioctx->io_sem, 1);
	}

	inc_vnode_ref_count(base_vnode, false);
	
	err = base_vnode->mount->fs->calls->fs_mkdir(base_vnode->mount->cookie, base_vnode->priv_vnode, path);	

	dec_vnode_ref_count(base_vnode, false, true);

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

