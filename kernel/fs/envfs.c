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

#include <kernel/arch/cpu.h>

#include <kernel/fs/envfs.h>

#include <libc/string.h>
#include <libc/printf.h>

struct env_key_value {
	char *value;
	int value_len;
	int ref_count;
};

struct envfs_stream {
	char *name;
	stream_type type;
	union {
		struct stream_dir {
			struct envfs_vnode *dir_head;
		} dir;
		struct stream_string {
			struct env_key_value *key;
		} string;
	} u;
	struct envfs_stream *next;
};

struct envfs_vnode {
	struct envfs_vnode *all_next;
	struct envfs_stream *stream_list;
	int id;
	char *name;
	void *redir_vnode;
	struct envfs_vnode *parent;
	struct envfs_vnode *dir_next;	
};

struct envfs {
	fs_id id;
	mutex lock;
	int next_vnode_id;
	void *covered_vnode;
	void *vnode_list_hash;
	struct envfs_vnode *root_vnode;
};

struct envfs_cookie {
	struct envfs_stream *s;
	union {
		struct cookie_dir {
			struct envfs_vnode *ptr;
		} dir;
		struct cookie_string {
			// nothing here
		} file;
	} u;
};

#define BOOTFS_HASH_SIZE 16
static unsigned int envfs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct envfs_vnode *v = _v;
	struct envfs_vnode *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return key->id % range;
}

static struct env_key_value *envfs_create_env_key(char *value)
{
	struct env_key_value *new_struct;
	
	new_struct = (struct env_key_value *)kmalloc(sizeof(struct env_key_value));
	if(new_struct == NULL)
		return NULL;

	new_struct->value = value;
	new_struct->value_len = strlen(value)+1;
	return new_struct;
}

static void envfs_inc_key_ref_count(struct env_key_value *key)
{
	key->ref_count++;
}

static void envfs_dec_key_ref_count(struct env_key_value *key)
{
	key->ref_count--;
	if(key->ref_count == 0) {
		if(key->value) kfree(key->value);
		kfree(key);
	}
}

static struct envfs_stream *envfs_create_stream(const char *name, stream_type type)
{
	struct envfs_stream *stream;
	
	stream = (struct envfs_stream *)kmalloc(sizeof(struct envfs_stream));
	if(stream == NULL)
		return NULL;
	
	stream->name = kmalloc(strlen(name) + 1);
	if(stream->name == NULL) {
		kfree(stream);
		return NULL;
	}
	strcpy(stream->name, name);
	
	stream->type = type;
	return stream;
}

static void envfs_insert_stream_into_vnode(struct envfs_stream *stream, struct envfs_vnode *v)
{
	stream->next = v->stream_list;
	v->stream_list = stream;
}

static void envfs_remove_stream_from_vnode(struct envfs_stream *stream, struct envfs_vnode *v)
{
	struct envfs_stream *s, *last = NULL;
	
	for(s = v->stream_list; s != NULL; last = s, s = s->next) {
		if(s == stream) {
			if(last == NULL) {
				v->stream_list = s->next;
			} else {
				last->next = s;
			}
			break;
		}
	}
}

static struct envfs_vnode *envfs_create_vnode(struct envfs *fs, const char *name)
{
	struct envfs_vnode *v;
	
	v = kmalloc(sizeof(struct envfs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct envfs_vnode));
	v->id = fs->next_vnode_id++;

	v->name = kmalloc(strlen(name) + 1);
	if(v->name == NULL) {
		kfree(v);
		return NULL;
	}
	strcpy(v->name, name);

	return v;
}

static int envfs_delete_vnode(struct envfs *fs, struct envfs_vnode *v, bool force_delete)
{
	struct envfs_stream *stream;

	// cant delete it if it's in a directory or is a directory
	// and has children
	if(!force_delete && ((v->stream_list->type == STREAM_TYPE_DIR && v->stream_list->u.dir.dir_head != NULL) || v->dir_next != NULL)) {
		return ERR_NOT_ALLOWED;
	}

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	// remove all of the streams
	while(v->stream_list != NULL) {
		stream = v->stream_list;
		v->stream_list = stream->next;
		
		if(stream->name != NULL)
			kfree(stream->name);
		if(stream->type == STREAM_TYPE_STRING)
			envfs_dec_key_ref_count(stream->u.string.key);
	}		
	kfree(v);

	return 0;
}

static struct envfs_vnode *envfs_find_in_dir(struct envfs_vnode *dir, const char *path, int start, int end)
{
	struct envfs_vnode *v;

	if(dir->stream_list->type != STREAM_TYPE_DIR)
		return NULL;

	v = dir->stream_list->u.dir.dir_head;
	while(v != NULL) {
//		dprintf("envfs_find_in_dir: looking at entry '%s'\n", v->name);
		if(strncmp(v->name, &path[start], end - start) == 0) {
//			dprintf("envfs_find_in_dir: found it at 0x%x\n", v);
			return v;
		}
		v = v->dir_next;
	}
	return NULL;
}

static int envfs_insert_in_dir(struct envfs_vnode *dir, struct envfs_vnode *v)
{
	if(dir->stream_list->type != STREAM_TYPE_DIR)
		return ERR_INVALID_ARGS;

	v->dir_next = dir->stream_list->u.dir.dir_head;
	dir->stream_list->u.dir.dir_head = v;
	
	v->parent = dir;
	return 0;
}

static struct envfs_stream *
envfs_get_stream_from_vnode(struct envfs_vnode *v, const char *stream_name, stream_type stream_type)
{
	struct envfs_stream *stream;
	
	for(stream = v->stream_list; stream != NULL; stream = stream->next) {
		if(stream->type == stream_type && strcmp(stream->name, stream_name))
			return stream;
	}
	return NULL;
}

// get the vnode this path represents, or sets the redir boolean to true if it hits a registered mountpoint
// and returns the vnode containing the mountpoint
static struct envfs_vnode *
envfs_get_vnode_from_path(struct envfs *fs, struct envfs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct envfs_vnode *cur_vnode;
	struct envfs_stream *s;
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		s = envfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR);
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
		
		cur_vnode = envfs_find_in_dir(cur_vnode, path, *start, end);
		if(cur_vnode == NULL)
			return NULL;
		*start = end;
	}

	if(cur_vnode->redir_vnode != NULL)
		*redir = true;
	return cur_vnode;
}

// get the vnode that would hold the last path entry. Same as above, but returns one path entry from the end
static struct envfs_vnode *
envfs_get_container_vnode_from_path(struct envfs *fs, struct envfs_vnode *base, const char *path, int *start, bool *redir)
{
	int last_start = 0;
	int end = 0;
	struct envfs_vnode *cur_vnode;
	struct envfs_vnode *last_vnode = NULL;
	
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
		if(cur_vnode == NULL || envfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR) == NULL)
			return NULL;
		cur_vnode = envfs_find_in_dir(cur_vnode, path, *start, end);
		*start = end;
	}

	if(last_vnode != NULL && last_vnode->redir_vnode != NULL)
		*redir = true;
	*start = last_start;
	return last_vnode;
}

int envfs_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct envfs *fs;
	struct envfs_vnode *v;
	int err;

	dprintf("envfs_mount: entry\n");

	fs = kmalloc(sizeof(struct envfs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	
	fs->covered_vnode = covered_vnode;
	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->lock, "envfs_mutex");
	if(err < 0) {
		goto err1;
	}
	
	fs->vnode_list_hash = hash_init(BOOTFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		NULL, &envfs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// create a vnode
	v = envfs_create_vnode(fs, "");
	if(v == NULL) {
		err = ERR_NO_MEMORY;
		goto err3;
	}

	// set it up	
	v->parent = v;
	
	v->stream_list = envfs_create_stream("", STREAM_TYPE_DIR);
	if(v->stream_list == NULL) {
		err = ERR_NO_MEMORY;
		goto err4;
	}
	v->stream_list->u.dir.dir_head = NULL;
	fs->root_vnode = v;

	hash_insert(fs->vnode_list_hash, v);

	*root_vnode = v;
	*fs_cookie = fs;
	
	return 0;
	
err4:
	envfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:	
	kfree(fs);
err:
	return err;
}

int envfs_unmount(void *_fs)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *v;
	struct hash_iterator i;
	
	dprintf("envfs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct envfs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		envfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);
	
	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	kfree(fs);
	
	return 0;
}

int envfs_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct envfs_vnode *v = _v;
	
	dprintf("envfs_register_mountpoint: vnode 0x%x covered by 0x%x\n", v, redir_vnode);
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

int envfs_unregister_mountpoint(void *_fs, void *_v)
{
	struct envfs_vnode *v = _v;
	
	dprintf("envfs_unregister_mountpoint: removing coverage at vnode 0x%x\n", v);

	v->redir_vnode = NULL;
	
	return 0;
}

int envfs_dispose_vnode(void *_fs, void *_v)
{
	dprintf("envfs_dispose_vnode: entry\n");

	// since vfs vnode == envfs vnode, we can't dispose of it
	return 0;
}

int envfs_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *base = _base_vnode;
	struct envfs_vnode *v;
	struct envfs_cookie *cookie;
	struct envfs_stream *s;
	int err = 0;
	int start = 0;

	dprintf("envfs_open: path '%s', stream '%s', stream_type %d, base_vnode 0x%x\n", path, stream, stream_type, base);

	mutex_lock(&fs->lock);

	v = envfs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
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
	
	s = envfs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	
	cookie = kmalloc(sizeof(struct envfs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	cookie->s = s;
	switch(stream_type) {
		case STREAM_TYPE_DIR:
			cookie->u.dir.ptr = s->u.dir.dir_head;
			break;
		case STREAM_TYPE_STRING:
			// do nothing
			break;
		default:
			dprintf("envfs_open: unhandled stream type\n");
	}

	*_cookie = cookie;
	*_vnode = v;	

err:	
	mutex_unlock(&fs->lock);

	return err;
}

int envfs_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *v = _vnode;
	struct envfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("envfs_seek: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, seek_type %d\n", v, cookie, pos, seek_type);
	
	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR:
			switch(seek_type) {
				// only valid args are seek_type SEEK_SET, pos 0.
				// this rewinds to beginning of directory
				case SEEK_SET:
					if(pos == 0) {
						cookie->u.dir.ptr = cookie->s->u.dir.dir_head;
					} else {
						err = ERR_INVALID_ARGS;
					}
					break;
				case SEEK_CUR:
				case SEEK_END:
				default:
					err = ERR_INVALID_ARGS;
			}
		case STREAM_TYPE_STRING:
			// do nothing
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&fs->lock);

	return err;
}

int envfs_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *v = _vnode;
	struct envfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("envfs_read: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x (0x%x)\n", v, cookie, pos, len, *len);

	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR: {
			dprintf("envfs_read: cookie is type DIR\n");

			if(cookie->u.dir.ptr == NULL) {
				*len = 0;
				goto err;
			}

			if(strlen(cookie->u.dir.ptr->name) + 1 > *len) {
				*len = 0;
				err = ERR_VFS_INSUFFICIENT_BUF;
				goto err;
			}
			
			strcpy(buf, cookie->u.dir.ptr->name);
			*len = strlen(cookie->u.dir.ptr->name) + 1;
			
			cookie->u.dir.ptr = cookie->u.dir.ptr->dir_next;
			break;
		}
		case STREAM_TYPE_STRING:
			dprintf("envfs_read: cookie is type STRING\n");

			if(*len <= 0) {
				*len = 0;
				break;
			}
			if(pos < 0) {
				pos = 0;
			}
			if(cookie->s->u.string.key == NULL) {
				// null value
				*len = 0;
				break;
			}
			if(pos >= cookie->s->u.string.key->value_len) {
				*len = 0;
				break;
			}
			if(pos + *len > cookie->s->u.string.key->value_len) {
				// trim the read
				*len = cookie->s->u.string.key->value_len - pos;
			}
			memcpy(buf, cookie->s->u.string.key->value + pos, *len);
			break;
		default:
			err = ERR_INVALID_ARGS;
	}
err:
	mutex_unlock(&fs->lock);

	return err;
}

int envfs_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *v = _vnode;
	struct envfs_cookie *cookie = _cookie;
	int err = 0;
	struct env_key_value *new_key;
	char *temp;

	dprintf("envfs_write: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x (0x%x)\n", v, cookie, pos, len, *len);

	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_STRING:
			dprintf("envfs_write: cookie is type STRING\n");

			if(*len <= 0) {
				*len = 0;
				break;
			}
			if(*len > 4095) {
				err = ERR_TOO_BIG;
				break;
			}
			if(pos < 0) {
				pos = 0;
			}
			// copy the data into a new buffer
			temp = kmalloc(*len + 1);
			if(temp == NULL) {
				err = ERR_NO_MEMORY;
				break;
			}
			memcpy(temp, buf, *len);
			temp[*len+1] = 0;
			
			// allocate a new env key
			new_key = envfs_create_env_key(temp);
			if(new_key == NULL) {
				kfree(temp);
				err = ERR_NO_MEMORY;
				break;
			}
			envfs_inc_key_ref_count(new_key);
			if(cookie->s->u.string.key != NULL)
				envfs_dec_key_ref_count(cookie->s->u.string.key);
			cookie->s->u.string.key = new_key;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}
err:
	mutex_unlock(&fs->lock);

	return err;
}

int envfs_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

int envfs_close(void *_fs, void *_vnode, void *_cookie)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *v = _vnode;
	struct envfs_cookie *cookie = _cookie;

	dprintf("envfs_close: entry vnode 0x%x, cookie 0x%x\n", v, cookie);

	if(cookie)
		kfree(cookie);

	return 0;
}

int envfs_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *base = _base_vnode;
	struct envfs_vnode *dir;
	struct envfs_vnode *new_vnode;
	struct envfs_stream *s;
	int err = 0;
	int start = 0;
	bool created_vnode = false;

	dprintf("envfs_create: path '%s', stream '%s', stream_type %d, base_vnode 0x%x\n", path, stream, stream_type, base);

	mutex_lock(&fs->lock);

	dir = envfs_get_container_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(dir == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		// loop back into the vfs because the parse hit a mount point
		mutex_unlock(&fs->lock);
		redir->vnode = dir->redir_vnode;
		redir->path = &path[start];
		return 0;
	}

	// we only support stream types of STREAM_TYPE_STRING
	if(stream_type != STREAM_TYPE_STRING) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	new_vnode = envfs_find_in_dir(dir, path, start, strlen(path) - 1);
	if(new_vnode == NULL) {
//		dprintf("rootfs_create: creating new vnode\n");
		new_vnode = envfs_create_vnode(fs, &path[start]);
		if(new_vnode == NULL) {
			err = ERR_NO_MEMORY;
			goto err;
		}
		created_vnode = true;
		new_vnode->parent = dir;
		envfs_insert_in_dir(dir, new_vnode);

		hash_insert(fs->vnode_list_hash, new_vnode);
		
		// create the new stream and insert it into the vnode
		s = envfs_create_stream(stream, stream_type);
		if(s == NULL) {
			err = ERR_NO_MEMORY;
			goto err1;
		}
		envfs_insert_stream_into_vnode(s, new_vnode);
	} else {
		// we found the vnode
//		dprintf("rootfs_create: found the vnode, possibly creating new stream\n");
		
		s = envfs_get_stream_from_vnode(new_vnode, stream, stream_type);
		if(s != NULL) {
			// it already exists, so lets bail
			err = ERR_VFS_ALREADY_EXISTS;
			goto err;
		} else {
			// we need to create it
			s = envfs_create_stream(stream, stream_type);
			if(s == NULL) {
				err = ERR_NO_MEMORY;
				goto err1;
			}
			envfs_insert_stream_into_vnode(s, new_vnode);
		}
	}

	switch(s->type) {	
		case STREAM_TYPE_STRING:
			s->u.string.key = NULL;
			break;
		default:
			; // not supported
	}

	mutex_unlock(&fs->lock);
	return 0;

err1:
	if(created_vnode)
		envfs_delete_vnode(fs, new_vnode, false);
err:
	mutex_unlock(&fs->lock);
	
	return err;	
}

int envfs_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct envfs *fs = _fs;
	struct envfs_vnode *base = _base_vnode;
	struct envfs_vnode *dir;
	struct envfs_vnode *v;
	struct envfs_stream *s;
	int err = 0;
	int start = 0;
	bool created_vnode = false;

	dprintf("envfs_stat: path '%s', stream '%s', stream_type %d, base_vnode 0x%x\n", path, stream, stream_type, base);

	mutex_lock(&fs->lock);

	dir = envfs_get_container_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(dir == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		// loop back into the vfs because the parse hit a mount point
		mutex_unlock(&fs->lock);
		redir->vnode = dir->redir_vnode;
		redir->path = &path[start];
		return 0;
	}

	// we only support stream types of STREAM_TYPE_STRING
	if(stream_type != STREAM_TYPE_STRING) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	v = envfs_find_in_dir(dir, path, start, strlen(path) - 1);
	if(v) {
		// we found it, size 0
		stat->size = 0;
		err = 0;
	} else {
		err = ERR_VFS_PATH_NOT_FOUND;
	}

err:
	mutex_unlock(&fs->lock);

	return err;
}

static struct fs_calls envfs_calls = {
	&envfs_mount,
	&envfs_unmount,
	&envfs_register_mountpoint,
	&envfs_unregister_mountpoint,
	&envfs_dispose_vnode,
	&envfs_open,
	&envfs_seek,
	&envfs_read,
	&envfs_write,
	&envfs_ioctl,
	&envfs_close,
	&envfs_create,
	&envfs_stat,
};

int bootstrap_envfs()
{
	dprintf("bootstrap_envfs: entry\n");
	
	return vfs_register_filesystem("envfs", &envfs_calls);
}
