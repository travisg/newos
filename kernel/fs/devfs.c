#include <string.h>
#include <printf.h>

#include <kernel.h>
#include <vfs.h>
#include <debug.h>
#include <khash.h>
#include <vm.h>
#include <sem.h>
#include <dev.h>

#include "devfs.h"

struct devfs_vnode {
	struct devfs_vnode *all_next;
	int id;
	char *name;
	file_type type;
	struct device_hooks *hooks;
	void *redir_vnode;
	struct devfs_vnode *dir_head;
	struct devfs_vnode *parent;
	struct devfs_vnode *dir_next;
};

struct devfs {
	fs_id id;
	sem_id sem;
	int next_vnode_id;
	void *covered_vnode;
	void *vnode_list_hash;
	struct devfs_vnode *root_vnode;
};

// only one instance of devfs can be mounted systemwide.
// a pointer to it is here.
static struct devfs *system_devfs;

struct devfs_dircookie {
	struct devfs_vnode *ptr;
};

#define ROOTFS_HASH_SIZE 16
static int devfs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct devfs_vnode *v = _v;
	struct devfs_vnode *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return key->id % range;
}

static struct devfs_vnode *devfs_create_vnode(struct devfs *fs)
{
	struct devfs_vnode *v;
	
	v = kmalloc(sizeof(struct devfs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct devfs_vnode));
	v->id = fs->next_vnode_id++;

	return v;
}

static int devfs_delete_vnode(struct devfs *fs, struct devfs_vnode *v, bool force_delete)
{
	// cant delete it if it's in a directory or is a directory
	// and has children
	if(!force_delete && (v->dir_head != NULL || v->dir_next != NULL)) {
		return -1;
	}

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	if(v->name != NULL)
		kfree(v->name);
	kfree(v);

	return 0;
}

static struct devfs_vnode *devfs_find_in_dir(struct devfs_vnode *dir, const char *path, int start, int end)
{
	struct devfs_vnode *v;

	v = dir->dir_head;
	while(v != NULL) {
//		dprintf("devfs_find_in_dir: looking at entry '%s'\n", v->name);
		if(strncmp(v->name, &path[start], end - start) == 0) {
//			dprintf("devfs_find_in_dir: found it at 0x%x\n", v);
			return v;
		}
		v = v->dir_next;
	}
	return NULL;
}

static int devfs_insert_in_dir(struct devfs_vnode *dir, struct devfs_vnode *v)
{
	v->dir_next = dir->dir_head;
	dir->dir_head = v;
	return 0;
}

// get the vnode this path represents, or sets the redir boolean to true if it hits a registered mountpoint
// and returns the vnode containing the mountpoint
static struct devfs_vnode *
devfs_get_vnode_from_path(struct devfs *fs, struct devfs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct devfs_vnode *cur_vnode;
	
	TOUCH(fs);
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		if(cur_vnode->type != FILE_TYPE_DIR)
			return NULL;

		if(cur_vnode->redir_vnode != NULL) {
			// we are at a mountpoint, redirect here
			*redir = true;
			return cur_vnode;
		}	
	
		if(*start == end) {
			// zero length path component, assume this means '.'
			return cur_vnode;
		}
		
		cur_vnode = devfs_find_in_dir(cur_vnode, path, *start, end);
		if(cur_vnode == NULL)
			return NULL;
		*start = end;
	}

	if(cur_vnode->redir_vnode != NULL)
		*redir = true;
	return cur_vnode;
}

// get the vnode that would hold the last path entry. Same as above, but returns one path entry from the end
static struct devfs_vnode *
devfs_get_container_vnode_from_path(struct devfs *fs, struct devfs_vnode *base, const char *path, int *start, bool *redir)
{
	int last_start = 0;
	int end = 0;
	struct devfs_vnode *cur_vnode;
	struct devfs_vnode *last_vnode = NULL;
	
	TOUCH(fs);
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		if(last_vnode != NULL && last_vnode->redir_vnode != NULL) {
			// we are at a mountpoint, redirect here
			*redir = true;
			*start = last_start;
			return last_vnode;
		}	
	
		last_start = *start;

		if(*start == end) {
			// zero length path component, assume this means '.'
			return last_vnode;
		}
		
		last_vnode = cur_vnode;
		if(cur_vnode == NULL || cur_vnode->type != FILE_TYPE_DIR)
			return NULL;
		cur_vnode = devfs_find_in_dir(cur_vnode, path, *start, end);
		*start = end;
	}

	if(last_vnode != NULL && last_vnode->redir_vnode != NULL)
		*redir = true;
	*start = last_start;
	return last_vnode;
}

int devfs_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct devfs *fs;
	struct devfs_vnode *v;
	int err;

	TOUCH(fs_cookie);
	TOUCH(flags);
	TOUCH(id);
	TOUCH(root_vnode);

	dprintf("devfs_mount: entry\n");

	fs = kmalloc(sizeof(struct devfs));
	if(fs == NULL) {
		err = -1;
		goto err;
	}
	
	fs->covered_vnode = covered_vnode;
	fs->id = id;
	fs->next_vnode_id = 0;

	{
		char temp[64];
		sprintf(temp, "devfs_sem%d", id);
		
		fs->sem = sem_create(1, temp);
		if(fs->sem < 0) {
			err = -1;
			goto err1;
		}
	}
	
	fs->vnode_list_hash = hash_init(ROOTFS_HASH_SIZE, (int)&v->all_next - (int)&v,
		NULL, &devfs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = -1;
		goto err2;
	}
	v = devfs_create_vnode(fs);
	if(v == NULL) {
		err = -1;
		goto err3;
	}
	
	v->parent = v;
	v->type = FILE_TYPE_DIR;
	v->name = kmalloc(strlen("") + 1);
	if(v->name == NULL) {
		err = -1;
		goto err4;
	}
	strcpy(v->name, "");
	
	*root_vnode = v;
	*fs_cookie = fs;
	
	return 0;
	
err4:
	devfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	sem_delete(fs->sem);
err1:	
	kfree(fs);
err:
	return err;
}

int devfs_unmount(void *_fs)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *v;
	void *hash_iterator;
	
	dprintf("devfs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_iterator = hash_open(fs->vnode_list_hash);
	while((v = (struct devfs_vnode *)hash_next(fs->vnode_list_hash, hash_iterator)) != NULL) {
		devfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, hash_iterator);
	
	hash_uninit(fs->vnode_list_hash);
	sem_delete(fs->sem);
	kfree(fs);
	
	return 0;
}

int devfs_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct devfs_vnode *v = _v;
	TOUCH(_fs);
	
	dprintf("devfs_register_mountpoint: vnode 0x%x covered by 0x%x\n", v, redir_vnode);
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

int devfs_unregister_mountpoint(void *_fs, void *_v)
{
	struct devfs_vnode *v = _v;
	TOUCH(_fs);
	
	dprintf("devfs_unregister_mountpoint: removing coverage at vnode 0x%x\n", v);

	v->redir_vnode = NULL;
	
	return 0;
}

int devfs_dispose_vnode(void *_fs, void *_v)
{
	TOUCH(_fs);TOUCH(_v);

	dprintf("devfs_dispose_vnode: entry on 0x%x\n", _v);

	// since vfs vnode == devfs vnode, we can't dispose of it
	return 0;
}

int devfs_opendir(void *_fs, void *_base_vnode, const char *path, void **_vnode, void **_dircookie)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *base = _base_vnode;
	struct devfs_vnode *v;
	struct devfs_dircookie *cookie;
	int err = 0;
	bool redir;
	int start = 0;

	dprintf("devfs_opendir: path '%s', base_vnode 0x%x\n", path, base);

	sem_acquire(fs->sem, 1);

	v = devfs_get_vnode_from_path(fs, base, path, &start, &redir);
	if(v == NULL) {
		err = -1;
		goto err;
	}
	if(redir == true) {
		// loop back into the vfs because the parse hit a mount point
		sem_release(fs->sem, 1);
		return vfs_opendir_loopback(v->redir_vnode, &path[start], _vnode, _dircookie);
	}
	
	cookie = kmalloc(sizeof(struct devfs_dircookie));
	if(cookie == NULL) {
		err = -1;
		goto err;
	}

	cookie->ptr = v->dir_head;

	*_dircookie = cookie;
	*_vnode = v;	

err:	
	sem_release(fs->sem, 1);

	return err;
}

int devfs_readdir(void *_fs, void *_dir_vnode, void *_dircookie, void *buf, unsigned int *buf_len)
{
	struct devfs *fs = _fs;
	struct devfs_dircookie *cookie = _dircookie;
	int err = 0;
	struct dirent *de = buf;

	TOUCH(_dir_vnode);

	dprintf("devfs_readdir: vnode 0x%x, cookie 0x%x\n", _dir_vnode, cookie);

	sem_acquire(fs->sem, 1);
	
	if(cookie->ptr == NULL) {
		*buf_len = 0;
		goto err;
	}
		
	if(sizeof(struct dirent) + strlen(cookie->ptr->name) + 1 > *buf_len) {
		*buf_len = 0;
		err = -1;
		goto err;
	}
	
	strcpy(de->name, cookie->ptr->name);
	de->name_len = strlen(cookie->ptr->name) + 1;
	*buf_len = sizeof(struct dirent) + de->name_len;
	
	cookie->ptr = cookie->ptr->dir_next;
	
err:
	sem_release(fs->sem, 1);

	return err;
}

int devfs_rewinddir(void *_fs, void *_dir_vnode, void *_dircookie)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *dir = _dir_vnode;
	struct devfs_dircookie *cookie = _dircookie;
	
	dprintf("devfs_rewinddir: entry vnode 0x%x, cookie 0x%x\n", dir, cookie);
	
	sem_acquire(fs->sem, 1);

	cookie->ptr = dir->dir_head;

	sem_release(fs->sem, 1);

	return 0;
}

int devfs_closedir(void *_fs, void *_dir_vnode)
{
	TOUCH(_fs);

	dprintf("devfs_closedir: entry vnode 0x%x\n", _dir_vnode);

	return 0;
}

int devfs_freedircookie(void *_fs, void *_dircookie)
{
	struct devfs_dircookie *cookie = _dircookie;
	TOUCH(_fs);
	
	dprintf("devfs_freedircookie: entry dircookie 0x%x\n", cookie);

	kfree(cookie);
	
	return 0;
}

int devfs_mkdir(void *_fs, void *_base_vnode, const char *path)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *base = _base_vnode;
	struct devfs_vnode *dir;
	struct devfs_vnode *new_vnode;
	bool redir;
	int start = 0;
	int err;
	
	dprintf("devfs_mkdir: base 0x%x, path = '%s'\n", base, path);
	
	sem_acquire(fs->sem, 1);
	
	dir = devfs_get_container_vnode_from_path(fs, base, path, &start, &redir);
	if(dir == NULL) {
		err = -1;
		goto err;
	}
	if(redir == true) {
		sem_release(fs->sem, 1);
		return vfs_mkdir_loopback(dir->redir_vnode, &path[start]);
	}
	dprintf("devfs_mkdir: found container at 0x%x\n", dir);
	
	if(devfs_find_in_dir(dir, path, start, strlen(path) - 1) == NULL) {
		new_vnode = devfs_create_vnode(fs);
		if(new_vnode == NULL) {
			err = -1;
			goto err;
		}
		new_vnode->name = kmalloc(strlen(&path[start]) + 1);
		if(new_vnode->name == NULL) {
			err = -1;
			goto err1;
		}
		strcpy(new_vnode->name, &path[start]);
		new_vnode->type = FILE_TYPE_DIR;
		new_vnode->parent = dir;
		devfs_insert_in_dir(dir, new_vnode);
	}
	sem_release(fs->sem, 1);
	return 0;

err1:
	devfs_delete_vnode(fs, new_vnode, false);
err:
	sem_release(fs->sem, 1);

	return err;
}

int devfs_create_device_node(const char *path, struct device_hooks *hooks)
{
	struct devfs *fs;
	struct devfs_vnode *dir;
	struct devfs_vnode *new_vnode = NULL;
	bool redir;
	int start = 0;
	int err;
	
	fs = system_devfs;
	if(fs == NULL)
		return -1;

	dprintf("devfs_create_device_node: path '%s', hooks 0x%x\n", path, hooks);
	
	sem_acquire(fs->sem, 1);
	
	dir = devfs_get_container_vnode_from_path(fs, fs->root_vnode, path, &start, &redir);
	if(dir == NULL) {
		err = -1;
		goto err;
	}
	// no redirects allowed here
	if(redir == true) {
		err = -1;
		goto err;
	}
	
	if(devfs_find_in_dir(dir, path, start, strlen(path) - 1) == NULL) {
		new_vnode = devfs_create_vnode(fs);
		if(new_vnode == NULL) {
			err = -1;
			goto err;
		}
		new_vnode->name = kmalloc(strlen(&path[start]) + 1);
		if(new_vnode->name == NULL) {
			err = -1;
			goto err1;
		}
		strcpy(new_vnode->name, &path[start]);
		new_vnode->type = FILE_TYPE_DEVICE;
		new_vnode->parent = dir;
		new_vnode->hooks = hooks;
		devfs_insert_in_dir(dir, new_vnode);
	}
	sem_release(fs->sem, 1);

	err = 0;

err1:
	devfs_delete_vnode(fs, new_vnode, false);
err:
	sem_release(fs->sem, 1);

	return err;
}

struct fs_calls devfs_calls = {
	&devfs_mount,
	&devfs_unmount,
	&devfs_register_mountpoint,
	&devfs_unregister_mountpoint,
	&devfs_dispose_vnode,
	&devfs_opendir,
	&devfs_readdir,
	&devfs_rewinddir,
	&devfs_closedir,
	&devfs_freedircookie,
	&devfs_mkdir,
};

int bootstrap_devfs()
{
	dprintf("bootstrap_devfs: entry\n");
	
	system_devfs = NULL;
	
	return vfs_register_filesystem("devfs", &devfs_calls);
}
