/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <sys/errors.h>

#include <kernel/fs/rootfs.h>

#include <libc/string.h>
#include <libc/printf.h>

struct rootfs_stream {
	char *name;
	stream_type type;
	// only type of stream supported by rootfs
	struct stream_dir {
		struct rootfs_vnode *dir_head;
	} dir;
};

struct rootfs_vnode {
	struct rootfs_vnode *all_next;
	int id;
	char *name;
	void *redir_vnode;
	struct rootfs_vnode *parent;
	struct rootfs_vnode *dir_next;
	struct rootfs_stream stream;
};

struct rootfs {
	fs_id id;
	mutex lock;
	int next_vnode_id;
	void *covered_vnode;
	void *vnode_list_hash;
	struct rootfs_vnode *root_vnode;
};

struct rootfs_cookie {
	// dircookie, dirs are only types of streams supported by rootfs
	struct rootfs_stream *s;
	struct rootfs_vnode *ptr;
};

#define ROOTFS_HASH_SIZE 16
static unsigned int rootfs_vnode_hash_func(void *_v, void *_key, int range)
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
	if(!force_delete && ((v->stream.type == STREAM_TYPE_DIR && v->stream.dir.dir_head != NULL) || v->dir_next != NULL)) {
		return ERR_NOT_ALLOWED;
	}

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	if(v->stream.name != NULL)
		kfree(v->stream.name);
	if(v->name != NULL)
		kfree(v->name);
	kfree(v);

	return 0;
}

static struct rootfs_vnode *rootfs_find_in_dir(struct rootfs_vnode *dir, const char *path, int start, int end)
{
	struct rootfs_vnode *v;

	if(dir->stream.type != STREAM_TYPE_DIR)
		return NULL;

	v = dir->stream.dir.dir_head;
	while(v != NULL) {
//		dprintf("rootfs_find_in_dir: looking at entry '%s'\n", v->name);
		if(strncmp(v->name, &path[start], end - start) == 0) {
//			dprintf("rootfs_find_in_dir: found it at 0x%x\n", v);
			return v;
		}
		v = v->dir_next;
	}
	return NULL;
}

static int rootfs_insert_in_dir(struct rootfs_vnode *dir, struct rootfs_vnode *v)
{
	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_INVALID_ARGS;

	v->dir_next = dir->stream.dir.dir_head;
	dir->stream.dir.dir_head = v;
	return 0;
}

static struct rootfs_stream *
rootfs_get_stream_from_vnode(struct rootfs_vnode *v, const char *stream, stream_type stream_type)
{
	// rootfs only supports a single stream that is:
	//   type STREAM_TYPE_DIR,
	//   named ""
//	dprintf("rootfs_get_stream_from_vnode: entry\n");

	if(stream_type != STREAM_TYPE_DIR || stream[0] != '\0')
		return NULL;
	
//	dprintf("1\n");
	
	if(v->stream.type != stream_type)
		return NULL;

//	dprintf("2\n");
	
	if(v->stream.name[0] != '\0')
		return NULL;

//	dprintf("rootfs_get_stream_from_vnode: returning 0x%x\n", &v->stream);
		
	return &v->stream;
}

// get the vnode this path represents, or sets the redir boolean to true if it hits a registered mountpoint
// and returns the vnode containing the mountpoint
static struct rootfs_vnode *
rootfs_get_vnode_from_path(struct rootfs *fs, struct rootfs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct rootfs_vnode *cur_vnode;
	struct rootfs_stream *s;
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		s = rootfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR);
		if(s == NULL)
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

	if(cur_vnode->redir_vnode != NULL)
		*redir = true;
	return cur_vnode;
}

// get the vnode that would hold the last path entry. Same as above, but returns one path entry from the end
static struct rootfs_vnode *
rootfs_get_container_vnode_from_path(struct rootfs *fs, struct rootfs_vnode *base, const char *path, int *start, bool *redir)
{
	int last_start = 0;
	int end = 0;
	struct rootfs_vnode *cur_vnode;
	struct rootfs_vnode *last_vnode = NULL;
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
//		dprintf("start = %d, end = %d\n", *start, end);
//		dprintf("cur = 0x%x, last = 0x%x\n", cur_vnode, last_vnode);

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
		if(cur_vnode == NULL || rootfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR) == NULL)
			return NULL;
		cur_vnode = rootfs_find_in_dir(cur_vnode, path, *start, end);
		*start = end;
	}

	if(last_vnode != NULL && last_vnode->redir_vnode != NULL)
		*redir = true;
	*start = last_start;
	return last_vnode;
}

int rootfs_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct rootfs *fs;
	struct rootfs_vnode *v;
	int err;

	dprintf("rootfs_mount: entry\n");

	fs = kmalloc(sizeof(struct rootfs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	
	fs->covered_vnode = covered_vnode;
	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->lock, "rootfs_mutex");
	if(err < 0) {
		goto err1;
	}
	
	fs->vnode_list_hash = hash_init(ROOTFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		NULL, &rootfs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// create a vnode
	v = rootfs_create_vnode(fs);
	if(v == NULL) {
		err = ERR_NO_MEMORY;
		goto err3;
	}

	// set it up	
	v->parent = v;
	v->name = kmalloc(strlen("") + 1);
	if(v->name == NULL) {
		err = ERR_NO_MEMORY;
		goto err4;
	}
	strcpy(v->name, "");
	
	// create a dir stream for it to hold
	v->stream.name = kmalloc(strlen("") + 1);
	if(v->stream.name == NULL) {
		err = ERR_NO_MEMORY;
		goto err4;
	}
	strcpy(v->stream.name, "");
	v->stream.type = STREAM_TYPE_DIR;
	v->stream.dir.dir_head = NULL;
	fs->root_vnode = v;
	hash_insert(fs->vnode_list_hash, v);

	*root_vnode = v;
	*fs_cookie = fs;
	
	return 0;
	
err4:
	rootfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:	
	kfree(fs);
err:
	return err;
}

int rootfs_unmount(void *_fs)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v;
	struct hash_iterator i;
	
	dprintf("rootfs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct rootfs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		rootfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);
	
	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	kfree(fs);
	
	return 0;
}

int rootfs_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct rootfs_vnode *v = _v;
	
	dprintf("rootfs_register_mountpoint: vnode 0x%x covered by 0x%x\n", v, redir_vnode);
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

int rootfs_unregister_mountpoint(void *_fs, void *_v)
{
	struct rootfs_vnode *v = _v;
	
	dprintf("rootfs_unregister_mountpoint: removing coverage at vnode 0x%x\n", v);

	v->redir_vnode = NULL;
	
	return 0;
}

int rootfs_dispose_vnode(void *_fs, void *_v)
{
	dprintf("rootfs_dispose_vnode: entry\n");

	// since vfs vnode == rootfs vnode, we can't dispose of it
	return 0;
}

int rootfs_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *base = _base_vnode;
	struct rootfs_vnode *v;
	struct rootfs_cookie *cookie;
	struct rootfs_stream *s;
	int err = 0;
	int start = 0;

	dprintf("rootfs_open: path '%s', stream '%s', stream_type %d, base_vnode 0x%x\n", path, stream, stream_type, base);

	mutex_lock(&fs->lock);

	v = rootfs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(v == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		// loop back into the vfs because the parse hit a mount point
		mutex_unlock(&fs->lock);
		redir->vnode = v->redir_vnode;
		redir->path = &path[start];
		return 0;
	}
	
	s = rootfs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	
	cookie = kmalloc(sizeof(struct rootfs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	cookie->s = s;
	cookie->ptr = s->dir.dir_head;

	*_cookie = cookie;
	*_vnode = v;	

err:	
	mutex_unlock(&fs->lock);

	return err;
}

int rootfs_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v = _vnode;
	struct rootfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("rootfs_seek: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, seek_type %d\n", v, cookie, pos, seek_type);
	
	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR:
			switch(seek_type) {
				// only valid args are seek_type SEEK_SET, pos 0.
				// this rewinds to beginning of directory
				case SEEK_SET:
					if(pos == 0) {
						cookie->ptr = cookie->s->dir.dir_head;
					} else {
						err = ERR_INVALID_ARGS;
					}
					break;
				case SEEK_CUR:
				case SEEK_END:
				default:
					err = ERR_INVALID_ARGS;
			}
		case STREAM_TYPE_FILE:
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&fs->lock);

	return err;
}

int rootfs_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v = _vnode;
	struct rootfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("rootfs_read: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x\n", v, cookie, pos, len);

	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR: {
			// only type really supported by rootfs
			if(cookie->ptr == NULL) {
				*len = 0;
				goto err;
			}

			if(strlen(cookie->ptr->name) + 1 > *len) {
				*len = 0;
				err = ERR_VFS_INSUFFICIENT_BUF;
				goto err;
			}
			
			strcpy(buf, cookie->ptr->name);
			*len = strlen(cookie->ptr->name) + 1;
			
			cookie->ptr = cookie->ptr->dir_next;
			break;
		}
		case STREAM_TYPE_FILE:
		default:
			err = ERR_INVALID_ARGS;
	}

err:
	mutex_unlock(&fs->lock);

	return err;
}

int rootfs_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return ERR_NOT_ALLOWED;
}

int rootfs_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

int rootfs_close(void *_fs, void *_vnode, void *_cookie)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *v = _vnode;
	struct rootfs_cookie *cookie = _cookie;

	dprintf("rootfs_close: entry vnode 0x%x, cookie 0x%x\n", v, cookie);

	if(cookie)
		kfree(cookie);

	return 0;
}

int rootfs_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *base = _base_vnode;
	struct rootfs_vnode *dir;
	struct rootfs_vnode *new_vnode;
	struct rootfs_stream *s;
	int start = 0;
	int err;
	bool created_vnode = false;

	dprintf("rootfs_create: base 0x%x, path = '%s', stream = '%s', stream_type = %d\n", base, path, stream, stream_type);
	
	mutex_lock(&fs->lock);
	
	dir = rootfs_get_container_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(dir == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		mutex_unlock(&fs->lock);
		redir->vnode = dir->redir_vnode;
		redir->path = &path[start];
		return 0;
	}
	// we only support stream types of STREAM_TYPE_DIR, and an unnamed one at that
	if(stream_type != STREAM_TYPE_DIR || stream[0] != '\0') {
		err = ERR_NOT_ALLOWED;
		goto err;
	}		

	new_vnode = rootfs_find_in_dir(dir, path, start, strlen(path) - 1);
	if(new_vnode == NULL) {
//		dprintf("rootfs_create: creating new vnode\n");
		new_vnode = rootfs_create_vnode(fs);
		if(new_vnode == NULL) {
			err = ERR_NO_MEMORY;
			goto err;
		}
		created_vnode = true;
		new_vnode->name = kmalloc(strlen(&path[start]) + 1);
		if(new_vnode->name == NULL) {
			err = ERR_NO_MEMORY;
			goto err1;
		}
		strcpy(new_vnode->name, &path[start]);
		new_vnode->parent = dir;
		rootfs_insert_in_dir(dir, new_vnode);

		hash_insert(fs->vnode_list_hash, new_vnode);
		
		s = &new_vnode->stream;
	} else {
		// we found the vnode
//		dprintf("rootfs_create: found the vnode, possibly creating new stream\n");
		
		s = rootfs_get_stream_from_vnode(new_vnode, stream, stream_type);
		if(s != NULL) {
			// it already exists, so lets bail
			err = ERR_VFS_ALREADY_EXISTS;
			goto err;
		} else {
			// we need to create it
			
			// It's already 'created', since it's statically stored in the vnode
			// right now it's being unused, or we would have found it
			s = &new_vnode->stream;
		}
	}
	
	// set up the new stream
	s->name = kmalloc(strlen(stream) + 1);
	if(s->name == NULL) {
		err = ERR_NO_MEMORY;
		goto err1;
	}
	strcpy(s->name, stream);
	s->type = stream_type;
	switch(s->type) {
		case STREAM_TYPE_DIR:
			s->dir.dir_head = NULL;
			break;
		case STREAM_TYPE_FILE:
		default:
			; // not supported
	}
	
	mutex_unlock(&fs->lock);
	return 0;

err1:
	if(created_vnode)
		rootfs_delete_vnode(fs, new_vnode, false);
err:
	mutex_unlock(&fs->lock);

	return err;
}

int rootfs_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct rootfs *fs = _fs;
	struct rootfs_vnode *base = _base_vnode;
	struct rootfs_vnode *v;
	struct rootfs_stream *s;
	int err = 0;
	int start = 0;

	dprintf("rootfs_stat: path '%s', stream '%s', stream_type %d, base_vnode 0x%x, stat 0x%x\n", path, stream, stream_type, base, stat);

	mutex_lock(&fs->lock);

	v = rootfs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(v == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		// loop back into the vfs because the parse hit a mount point
		mutex_unlock(&fs->lock);
		redir->vnode = v->redir_vnode;
		redir->path = &path[start];
		return 0;
	}
	
	s = rootfs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	// stream exists, but we know to return size 0, since we can only hold directories
	stat->size = 0;
	err = 0;

err:	
	mutex_unlock(&fs->lock);

	return err;
}

static struct fs_calls rootfs_calls = {
	&rootfs_mount,
	&rootfs_unmount,
	&rootfs_register_mountpoint,
	&rootfs_unregister_mountpoint,
	&rootfs_dispose_vnode,
	&rootfs_open,
	&rootfs_seek,
	&rootfs_read,
	&rootfs_write,
	&rootfs_ioctl,
	&rootfs_close,
	&rootfs_create,
	&rootfs_stat,
};

int bootstrap_rootfs()
{
	dprintf("bootstrap_rootfs: entry\n");
	
	return vfs_register_filesystem("rootfs", &rootfs_calls);
}
