#include <string.h>
#include <printf.h>

#include <kernel.h>
#include <vfs.h>
#include <debug.h>
#include <khash.h>
#include <vm.h>
#include <sem.h>

#include "rootfs.h"

struct rootfs_vnode {
	struct rootfs_vnode *all_next;
	int id;
	char *name;
	file_type type;
	bool vfs_has;
	void *redir_vnode;
	struct rootfs_vnode *dir_head;
	struct rootfs_vnode *parent;
	struct rootfs_vnode *dir_next;
};

struct rootfs {
	fs_id id;
	sem_id sem;
	int next_vnode_id;
	void *vnode_list_hash;
	struct rootfs_vnode *root_vnode;
};

struct rootfs_dircookie {
	struct rootfs_vnode *ptr;
};

#define ROOTFS_HASH_SIZE 16
static int rootfs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct rootfs_vnode *v = _v;
	struct rootfs_vnode *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return key->id % range;
}

static struct rootfs_vnode *rootfs_create_vnode(struct rootfs *fs)
{
	struct rootfs_vnode *v;
	
	v = kmalloc(sizeof(struct rootfs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct rootfs_vnode));
	v->id = fs->next_vnode_id++;

	return v;
}

static int rootfs_delete_vnode(struct rootfs *fs, struct rootfs_vnode *v, bool force_delete)
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

static struct rootfs_vnode *rootfs_find_in_dir(struct rootfs_vnode *dir, const char *path, int start, int end)
{
	struct rootfs_vnode *v;

	v = dir->dir_head;
	while(v != NULL) {
		if(strncmp(v->name, &path[start], end - start) == 0) {
			return v;
		}
	}
	return NULL;
}

static struct rootfs_vnode *rootfs_get_vnode_from_path(struct rootfs *fs, struct rootfs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct rootfs_vnode *cur_vnode;
	
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
		
		cur_vnode = rootfs_find_in_dir(cur_vnode, path, *start, end);
		if(cur_vnode == NULL)
			return NULL;
		*start = end;
	}

	return NULL;
}

int rootfs_mount(void **fs_cookie, void *flags, fs_id id, void **root_vnode)
{
	struct rootfs *fs;
	struct rootfs_vnode *v;
	int err;

	TOUCH(fs_cookie);
	TOUCH(flags);
	TOUCH(id);
	TOUCH(root_vnode);

	dprintf("rootfs_mount: entry\n");

	fs = kmalloc(sizeof(struct rootfs));
	if(fs == NULL) {
		err = -1;
		goto err;
	}
	
	fs->id = id;
	fs->next_vnode_id = 0;

	{
		char temp[64];
		sprintf(temp, "rootfs_sem%d", id);
		
		fs->sem = sem_create(1, temp);
		if(fs->sem < 0) {
			err = -1;
			goto err1;
		}
	}
	
	fs->vnode_list_hash = hash_init(ROOTFS_HASH_SIZE, (int)&v->all_next - (int)&v,
		NULL, &rootfs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = -1;
		goto err2;
	}
	v = rootfs_create_vnode(fs);
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
	v->vfs_has = true;
	
	*root_vnode = v;
	*fs_cookie = fs;
	
	return 0;
	
err4:
	rootfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	sem_delete(fs->sem);
err1:	
	kfree(fs);
err:
	return err;
}

int rootfs_unmount(void *_fs)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v;
	void *hash_iterator;
	
	dprintf("rootfs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_iterator = hash_open(fs->vnode_list_hash);
	while((v = (struct rootfs_vnode *)hash_next(fs->vnode_list_hash, hash_iterator)) != NULL) {
		rootfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, hash_iterator);
	
	hash_uninit(fs->vnode_list_hash);
	dprintf("rootfs_unmount: deleting sem 0x%x\n", fs->sem);
	sem_delete(fs->sem);
	kfree(fs);
	
	return 0;
}

int rootfs_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct rootfs_vnode *v = _v;
	TOUCH(_fs);
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

int rootfs_unregister_mountpoint(void *_fs, void *_v)
{
	struct rootfs_vnode *v = _v;
	TOUCH(_fs);
	
	v->redir_vnode = NULL;
	
	return 0;
}

int rootfs_dispose_vnode(void *_fs, void *_v)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v = _v;

	dprintf("rootfs_dispose_vnode: entry\n");

	sem_acquire(fs->sem, 1);

	v->vfs_has = false;
	
	sem_release(fs->sem, 1);
	
	// since vfs vnode == rootfs vnode, we can't dispose of it
	return 0;
}

int rootfs_opendir(void *_fs, void *_base_vnode, const char *path, void **_vnode, void **_dircookie)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *base = _base_vnode;
	struct rootfs_vnode *v;
	struct rootfs_dircookie *cookie;
	int err = 0;
	bool redir;
	int start;

	dprintf("rootfs_opendir: path '%s', base_vnode 0x%x\n", path, base);

	sem_acquire(fs->sem, 1);

	v = rootfs_get_vnode_from_path(fs, base, path, &start, &redir);
	if(v == NULL) {
		err = -1;
		goto err;
	}
	if(redir == true) {
		// loop back into the vfs because the parse hit a mount point
		sem_release(fs->sem, 1);
		return vfs_opendir(&path[start], v);
	}
	
	cookie = kmalloc(sizeof(struct rootfs_dircookie));
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

int rootfs_rewinddir(void *_fs, void *_dir_vnode, void *_dircookie)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v = _dir_vnode;
	struct rootfs_dircookie *cookie = _dircookie;
	
	sem_acquire(fs->sem, 1);

	cookie->ptr = v->dir_head;

	sem_release(fs->sem, 1);

	return 0;
}

struct fs_calls rootfs_calls = {
	&rootfs_mount,
	&rootfs_unmount,
	&rootfs_register_mountpoint,
	&rootfs_unregister_mountpoint,
	&rootfs_dispose_vnode,
	&rootfs_opendir,
	&rootfs_rewinddir,
};

// XXX
int bootstrap_rootfs()
{
	dprintf("bootstrap_rootfs: entry\n");
	
	return vfs_register_filesystem("rootfs", &rootfs_calls);
}
