#include <string.h>

#include <kernel.h>
#include <stage2.h>
#include <vfs.h>
#include <debug.h>
#include <khash.h>
#include <sem.h>

#include <fs/rootfs.h>

struct vnode {
	struct vnode *next;
	vnode_id id;
};

struct fs_container {
	struct fs_container *next;
	const char *name;
	struct fs_calls *calls;
};
static struct fs_container *fs_list;

struct fs_mount {
	struct fs_mount *next;
	fs_id id;
	vnode_id root_vnid;
	void *cookie;
	char *mount_point;
	struct fs_container *fs;
};
static struct fs_mount *fs_mounts;

static fs_id next_fsid = 0;
static sem_id vfs_sem;

#define VNODE_HASH_TABLE_SIZE 1024
static void *vnode_table;

static int vnode_hash_compare(void *_v, void *_key)
{
	struct vnode *v = _v;
	vnode_id *vid = _key;

	if(v->id == *vid)
		return 0;
	return 1;
}

static int vnode_hash(void *_v, void *_key, int range)
{
	struct vnode *v = _v;
	vnode_id *vid = _key;
	
	if(v != NULL)
		return (v->id % range);
	else
		return (*vid % range);
}

static struct fs_container *find_fs(const char *fs_name)
{
	struct fs_container *fs = fs_list;
	while(fs != NULL) {
		if(strcmp(fs_name, fs->name) == 0) {
			return fs;
		}
	}
	return NULL;
}
	
int vfs_init(kernel_args *ka)
{
	TOUCH(ka);

	dprintf("vfs_init: entry\n");

	{
		struct vnode *v;
		
		vnode_table = hash_init(VNODE_HASH_TABLE_SIZE, (int)(&v->next - &v),
			&vnode_hash_compare, &vnode_hash);
		if(vnode_hash == NULL)
			panic("vfs_init: error creating vnode hash table\n");
	}
	fs_list = NULL;
	fs_mounts = NULL;

	vfs_sem = sem_create(1, "vfs_lock");
	if(vfs_sem < 0)
		panic("vfs_init: error allocating vfs lock\n");

	// bootstrap a few filesystems
	bootstrap_rootfs();	

	vfs_mount("/", "rootfs");

	return 0;
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
	struct fs_container *fs;
	int err = 0;

	mount = (struct fs_mount *)kmalloc(sizeof(struct fs_mount));
	if(mount == NULL)
		return -1;

	sem_acquire(vfs_sem, 1);

	fs = find_fs(fs_name);
	if(fs == NULL) {
		kfree(mount);
		err = -1;
		goto err;
	}

	mount->id = next_fsid++;

	if(fs_mounts == NULL) {
		// we haven't mounted anything yet	
		if(strcmp(path, "/") != 0) {
			err = -1;
			goto err;
		}
		err = fs->calls->fs_mount(&mount->cookie, 0, mount->id, &mount->root_vnid);
		if(err < 0) {
			kfree(mount);
			goto err;
		}			
	}

	// mount was successful
	mount->mount_point = (char *)kmalloc(strlen(path));
	if(mount->mount_point == NULL) {
		// XXX unmount here
		kfree(mount);
		err = -1;
		goto err;
	}
	strcpy(mount->mount_point, path);
	mount->next = fs_mounts;
	fs_mounts = mount;
	

err:
	sem_release(vfs_sem, 1);

	return err;
} 
