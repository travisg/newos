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

#include <kernel/fs/bootfs.h>

#include <boot/bootdir.h>

#include <libc/string.h>
#include <libc/printf.h>

static char *bootdir = NULL;
static off_t bootdir_len = 0;
static region_id bootdir_region = -1;

struct bootfs_stream {
	char *name;
	stream_type type;
	union {
		struct stream_dir {
			struct bootfs_vnode *dir_head;
		} dir;
		struct stream_file {
			void *start;
			off_t len;
		} file;
	} u;
};

struct bootfs_vnode {
	struct bootfs_vnode *all_next;
	int id;
	char *name;
	void *redir_vnode;
	struct bootfs_vnode *parent;
	struct bootfs_vnode *dir_next;
	struct bootfs_stream stream;
};

struct bootfs {
	fs_id id;
	mutex lock;
	int next_vnode_id;
	void *covered_vnode;
	void *vnode_list_hash;
	struct bootfs_vnode *root_vnode;
};

struct bootfs_cookie {
	struct bootfs_stream *s;
	union {
		struct cookie_dir {
			struct bootfs_vnode *ptr;
		} dir;
		struct cookie_file {
			off_t pos;
		} file;
	} u;
};

#define BOOTFS_HASH_SIZE 16
static unsigned int bootfs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct bootfs_vnode *v = _v;
	struct bootfs_vnode *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return key->id % range;
}

static struct bootfs_vnode *bootfs_create_vnode(struct bootfs *fs, const char *name)
{
	struct bootfs_vnode *v;
	
	v = kmalloc(sizeof(struct bootfs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct bootfs_vnode));
	v->id = fs->next_vnode_id++;

	v->name = kmalloc(strlen(name) + 1);
	if(v->name == NULL) {
		kfree(v);
		return NULL;
	}
	strcpy(v->name, name);

	return v;
}

static int bootfs_delete_vnode(struct bootfs *fs, struct bootfs_vnode *v, bool force_delete)
{
	// cant delete it if it's in a directory or is a directory
	// and has children
	if(!force_delete && ((v->stream.type == STREAM_TYPE_DIR && v->stream.u.dir.dir_head != NULL) || v->dir_next != NULL)) {
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

static struct bootfs_vnode *bootfs_find_in_dir(struct bootfs_vnode *dir, const char *path, int start, int end)
{
	struct bootfs_vnode *v;

	if(dir->stream.type != STREAM_TYPE_DIR)
		return NULL;

	v = dir->stream.u.dir.dir_head;
	while(v != NULL) {
//		dprintf("bootfs_find_in_dir: looking at entry '%s'\n", v->name);
		if(strncmp(v->name, &path[start], end - start) == 0) {
//			dprintf("bootfs_find_in_dir: found it at 0x%x\n", v);
			return v;
		}
		v = v->dir_next;
	}
	return NULL;
}

static int bootfs_insert_in_dir(struct bootfs_vnode *dir, struct bootfs_vnode *v)
{
	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_INVALID_ARGS;

	v->dir_next = dir->stream.u.dir.dir_head;
	dir->stream.u.dir.dir_head = v;
	
	v->parent = dir;
	return 0;
}

static struct bootfs_stream *
bootfs_get_stream_from_vnode(struct bootfs_vnode *v, const char *stream, stream_type stream_type)
{
	if(v->stream.type != stream_type)
		return NULL;
	
	if(strcmp(stream, v->stream.name) != 0)
		return NULL;
		
	return &v->stream;
}

// get the vnode this path represents, or sets the redir boolean to true if it hits a registered mountpoint
// and returns the vnode containing the mountpoint
static struct bootfs_vnode *
bootfs_get_vnode_from_path(struct bootfs *fs, struct bootfs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct bootfs_vnode *cur_vnode;
	struct bootfs_stream *s;
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		s = bootfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR);
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
		
		cur_vnode = bootfs_find_in_dir(cur_vnode, path, *start, end);
		if(cur_vnode == NULL)
			return NULL;
		*start = end;
	}

	if(cur_vnode->redir_vnode != NULL)
		*redir = true;
	return cur_vnode;
}

// get the vnode that would hold the last path entry. Same as above, but returns one path entry from the end
static struct bootfs_vnode *
bootfs_get_container_vnode_from_path(struct bootfs *fs, struct bootfs_vnode *base, const char *path, int *start, bool *redir)
{
	int last_start = 0;
	int end = 0;
	struct bootfs_vnode *cur_vnode;
	struct bootfs_vnode *last_vnode = NULL;
	
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
		if(cur_vnode == NULL || bootfs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR) == NULL)
			return NULL;
		cur_vnode = bootfs_find_in_dir(cur_vnode, path, *start, end);
		*start = end;
	}

	if(last_vnode != NULL && last_vnode->redir_vnode != NULL)
		*redir = true;
	*start = last_start;
	return last_vnode;
}

int bootfs_create_vnode_tree(struct bootfs *fs, struct bootfs_vnode *v)
{
	int i;
	boot_entry *entry;
	int err;
	struct redir_struct redir;
	struct bootfs_vnode *new_vnode;
	
	entry = (boot_entry *)bootdir;
	for(i=0; i<BOOTDIR_MAX_ENTRIES; i++) {
		if(entry[i].be_type != BE_TYPE_NONE && entry[i].be_type != BE_TYPE_DIRECTORY) {
			new_vnode = bootfs_create_vnode(fs, entry[i].be_name);
			if(new_vnode == NULL)
				return ERR_NO_MEMORY;
			
			// set up the new node
			new_vnode->stream.name = kmalloc(strlen("") + 1);
			if(new_vnode->stream.name == NULL) {
				bootfs_delete_vnode(fs, new_vnode, true);
				return ERR_NO_MEMORY;
			}
			strcpy(new_vnode->stream.name, "");
			new_vnode->stream.type = STREAM_TYPE_FILE;
			new_vnode->stream.u.file.start = bootdir + entry[i].be_offset * PAGE_SIZE;
			new_vnode->stream.u.file.len = entry[i].be_size * PAGE_SIZE;

			dprintf("bootfs_create_vnode_tree: added entry '%s', start 0x%x, len 0x%x\n", new_vnode->name,
				new_vnode->stream.u.file.start, new_vnode->stream.u.file.len);

			// insert it into the parent dir
			bootfs_insert_in_dir(v, new_vnode);
	
			hash_insert(fs->vnode_list_hash, new_vnode);
		}
	}

	return 0;
}

int bootfs_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct bootfs *fs;
	struct bootfs_vnode *v;
	int err;

	dprintf("bootfs_mount: entry\n");

	fs = kmalloc(sizeof(struct bootfs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	
	fs->covered_vnode = covered_vnode;
	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->lock, "bootfs_mutex");
	if(err < 0) {
		goto err1;
	}
	
	fs->vnode_list_hash = hash_init(BOOTFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		NULL, &bootfs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// create a vnode
	v = bootfs_create_vnode(fs, "");
	if(v == NULL) {
		err = ERR_NO_MEMORY;
		goto err3;
	}

	// set it up	
	v->parent = v;
	
	// create a dir stream for it to hold
	v->stream.name = kmalloc(strlen("") + 1);
	if(v->stream.name == NULL) {
		err = ERR_NO_MEMORY;
		goto err4;
	}
	strcpy(v->stream.name, "");
	v->stream.type = STREAM_TYPE_DIR;
	v->stream.u.dir.dir_head = NULL;
	fs->root_vnode = v;

	hash_insert(fs->vnode_list_hash, v);

	err = bootfs_create_vnode_tree(fs, fs->root_vnode);
	if(err < 0) {
		goto err4;
	}

	*root_vnode = v;
	*fs_cookie = fs;
	
	return 0;
	
err4:
	bootfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:	
	kfree(fs);
err:
	return err;
}

int bootfs_unmount(void *_fs)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *v;
	struct hash_iterator i;
	
	dprintf("bootfs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct bootfs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		bootfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);
	
	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	kfree(fs);
	
	return 0;
}

int bootfs_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct bootfs_vnode *v = _v;
	
	dprintf("bootfs_register_mountpoint: vnode 0x%x covered by 0x%x\n", v, redir_vnode);
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

int bootfs_unregister_mountpoint(void *_fs, void *_v)
{
	struct bootfs_vnode *v = _v;
	
	dprintf("bootfs_unregister_mountpoint: removing coverage at vnode 0x%x\n", v);

	v->redir_vnode = NULL;
	
	return 0;
}

int bootfs_dispose_vnode(void *_fs, void *_v)
{
	dprintf("bootfs_dispose_vnode: entry\n");

	// since vfs vnode == bootfs vnode, we can't dispose of it
	return 0;
}

int bootfs_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *base = _base_vnode;
	struct bootfs_vnode *v;
	struct bootfs_cookie *cookie;
	struct bootfs_stream *s;
	int err = 0;
	int start = 0;

	dprintf("bootfs_open: path '%s', stream '%s', stream_type %d, base_vnode 0x%x\n", path, stream, stream_type, base);

	mutex_lock(&fs->lock);

	v = bootfs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
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
	
	s = bootfs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	
	cookie = kmalloc(sizeof(struct bootfs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	cookie->s = s;
	switch(stream_type) {
		case STREAM_TYPE_DIR:
			cookie->u.dir.ptr = s->u.dir.dir_head;
			break;
		case STREAM_TYPE_FILE:
			cookie->u.file.pos = 0;
			break;
		default:
			dprintf("bootfs_open: unhandled stream type\n");
	}

	*_cookie = cookie;
	*_vnode = v;	

err:	
	mutex_unlock(&fs->lock);

	return err;
}

int bootfs_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *v = _vnode;
	struct bootfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("bootfs_seek: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, seek_type %d\n", v, cookie, pos, seek_type);
	
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
			break;
		case STREAM_TYPE_FILE:
			switch(seek_type) {
				case SEEK_SET:
					if(pos < 0)
						pos = 0;
					if(pos > cookie->s->u.file.len)
						pos = cookie->s->u.file.len;
					cookie->u.file.pos = pos;
					break;
				case SEEK_CUR:
					if(pos + cookie->u.file.pos > cookie->s->u.file.len)
						cookie->u.file.pos = cookie->s->u.file.len;
					else if(pos + cookie->u.file.pos < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos += pos;
					break;
				case SEEK_END:
					if(pos > 0)
						cookie->u.file.pos = cookie->s->u.file.len;
					else if(pos + cookie->s->u.file.len < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos = pos + cookie->s->u.file.len;
					break;
				default:
					err = ERR_INVALID_ARGS;

			}
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&fs->lock);

	return err;
}

int bootfs_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *v = _vnode;
	struct bootfs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("bootfs_read: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x (0x%x)\n", v, cookie, pos, len, *len);

	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR: {
			dprintf("bootfs_read: cookie is type DIR\n");

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
		case STREAM_TYPE_FILE:
			dprintf("bootfs_read: cookie is type FILE\n");

			if(*len <= 0) {
				*len = 0;
				break;
			}
			if(pos < 0) {
				// we'll read where the cookie is at
				pos = cookie->u.file.pos;
			}
			if(pos >= cookie->s->u.file.len) {
				*len = 0;
				break;
			}
			if(pos + *len > cookie->s->u.file.len) {
				// trim the read
				*len = cookie->s->u.file.len - pos;
			}
			memcpy(buf, cookie->s->u.file.start + pos, *len);

			cookie->u.file.pos = pos + *len;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}
err:
	mutex_unlock(&fs->lock);

	return err;
}

int bootfs_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return ERR_VFS_READONLY_FS;
}

int bootfs_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_VFS_READONLY_FS;
}

int bootfs_close(void *_fs, void *_vnode, void *_cookie)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *v = _vnode;
	struct bootfs_cookie *cookie = _cookie;

	dprintf("bootfs_close: entry vnode 0x%x, cookie 0x%x\n", v, cookie);

	if(cookie)
		kfree(cookie);

	return 0;
}

int bootfs_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	return ERR_VFS_READONLY_FS;
}

int bootfs_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct bootfs *fs = _fs;
	struct bootfs_vnode *base = _base_vnode;
	struct bootfs_vnode *v;
	struct bootfs_stream *s;
	int err = 0;
	int start = 0;

	dprintf("bootfs_stat: path '%s', stream '%s', stream_type %d, base_vnode 0x%x, stat 0x%x\n", path, stream, stream_type, base, stat);

	mutex_lock(&fs->lock);

	v = bootfs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
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
	
	s = bootfs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	switch(stream_type) {
		case STREAM_TYPE_DIR:
			stat->size = 0;
			break;
		case STREAM_TYPE_FILE:
			stat->size = s->u.file.len;
			break;
		default:
			err = ERR_INVALID_ARGS;
			break;
	}

err:	
	mutex_unlock(&fs->lock);

	return err;
}

static struct fs_calls bootfs_calls = {
	&bootfs_mount,
	&bootfs_unmount,
	&bootfs_register_mountpoint,
	&bootfs_unregister_mountpoint,
	&bootfs_dispose_vnode,
	&bootfs_open,
	&bootfs_seek,
	&bootfs_read,
	&bootfs_write,
	&bootfs_ioctl,
	&bootfs_close,
	&bootfs_create,
	&bootfs_stat
};

int bootstrap_bootfs()
{
	region_id rid;
	vm_region_info rinfo;
	
	dprintf("bootstrap_bootfs: entry\n");
	
	// find the bootdir and set it up
	rid = vm_find_region_by_name(vm_get_kernel_aspace_id(), "bootdir");
	if(rid < 0)
		panic("bootstrap_bootfs: no bootdir area found!\n");
	vm_get_region_info(rid, &rinfo);
	bootdir = (char *)rinfo.base;
	bootdir_len = rinfo.size;
	bootdir_region = rinfo.id;
	
	dprintf("bootstrap_bootfs: found bootdir at 0x%x\n", bootdir);
	
	return vfs_register_filesystem("bootfs", &bootfs_calls);
}
