/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/vfs.h>
#include <kernel/khash.h>
#include <sys/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <libc/string.h>
#include <libc/printf.h>

#include <bus/bus.h>
#include <bus/pci/pci_bus.h>

#include "pci_p.h" // private includes

struct pcifs_stream {
	char *name;
	stream_type type;
	union {
		struct stream_dir {
			struct pcifs_vnode *dir_head;
		} dir;
		struct stream_dev {
			struct pci_cfg *cfg;
		} dev;
	} u;
};

struct pcifs_vnode {
	struct pcifs_vnode *all_next;
	int id;
	char *name;
	void *redir_vnode;
	struct pcifs_vnode *parent;
	struct pcifs_vnode *dir_next;
	struct pcifs_stream stream;
};

struct pcifs_cookie {
	struct pcifs_stream *s;
	union {
		struct cookie_dir {
			struct pcifs_vnode *ptr;
		} dir;
		struct cookie_dev {
			int null;
		} file;
	} u;
};

struct pcifs {
	fs_id id;
	mutex lock;
	int next_vnode_id;
	void *covered_vnode;
	void *vnode_list_hash;
	struct pcifs_vnode *root_vnode;
};

#define PCIFS_HASH_SIZE 16
static unsigned int pcifs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct pcifs_vnode *v = _v;
	struct pcifs_vnode *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return key->id % range;
}

static struct pcifs_vnode *pcifs_create_vnode(struct pcifs *fs, const char *name)
{
	struct pcifs_vnode *v;
	
	v = kmalloc(sizeof(struct pcifs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct pcifs_vnode));
	v->id = fs->next_vnode_id++;

	v->name = kmalloc(strlen(name) + 1);
	if(v->name == NULL) {
		kfree(v);
		return NULL;
	}
	strcpy(v->name, name);

	return v;
}

static int pcifs_delete_vnode(struct pcifs *fs, struct pcifs_vnode *v, bool force_delete)
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

static struct pcifs_vnode *pcifs_find_in_dir(struct pcifs_vnode *dir, const char *path, int start, int end)
{
	struct pcifs_vnode *v;

	if(dir->stream.type != STREAM_TYPE_DIR)
		return NULL;

	v = dir->stream.u.dir.dir_head;
	while(v != NULL) {
//		dprintf("pcifs_find_in_dir: looking at entry '%s'\n", v->name);
		if(strncmp(v->name, &path[start], end - start) == 0) {
//			dprintf("pcifs_find_in_dir: found it at 0x%x\n", v);
			return v;
		}
		v = v->dir_next;
	}
	return NULL;
}

static int pcifs_insert_in_dir(struct pcifs_vnode *dir, struct pcifs_vnode *v)
{
	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_INVALID_ARGS;

	v->dir_next = dir->stream.u.dir.dir_head;
	dir->stream.u.dir.dir_head = v;
	
	v->parent = dir;
	return 0;
}

static struct pcifs_stream *
pcifs_get_stream_from_vnode(struct pcifs_vnode *v, const char *stream, stream_type stream_type)
{
	if(v->stream.type != stream_type)
		return NULL;
	
	if(strcmp(stream, v->stream.name) != 0)
		return NULL;
		
	return &v->stream;
}

// get the vnode this path represents, or sets the redir boolean to true if it hits a registered mountpoint
// and returns the vnode containing the mountpoint
static struct pcifs_vnode *
pcifs_get_vnode_from_path(struct pcifs *fs, struct pcifs_vnode *base, const char *path, int *start, bool *redir)
{
	int end = 0;
	struct pcifs_vnode *cur_vnode;
	struct pcifs_stream *s;
	
	*redir = false;

	cur_vnode = base;
	while(vfs_helper_getnext_in_path(path, start, &end) > 0) {
		s = pcifs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR);
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
		
		cur_vnode = pcifs_find_in_dir(cur_vnode, path, *start, end);
		if(cur_vnode == NULL)
			return NULL;
		*start = end;
	}

	if(cur_vnode->redir_vnode != NULL)
		*redir = true;
	return cur_vnode;
}

// get the vnode that would hold the last path entry. Same as above, but returns one path entry from the end
static struct pcifs_vnode *
pcifs_get_container_vnode_from_path(struct pcifs *fs, struct pcifs_vnode *base, const char *path, int *start, bool *redir)
{
	int last_start = 0;
	int end = 0;
	struct pcifs_vnode *cur_vnode;
	struct pcifs_vnode *last_vnode = NULL;
	
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
		if(cur_vnode == NULL || pcifs_get_stream_from_vnode(cur_vnode, "", STREAM_TYPE_DIR) == NULL)
			return NULL;
		cur_vnode = pcifs_find_in_dir(cur_vnode, path, *start, end);
		*start = end;
	}

	if(last_vnode != NULL && last_vnode->redir_vnode != NULL)
		*redir = true;
	*start = last_start;
	return last_vnode;
}

static struct pcifs_vnode *pcifs_create_dir_if_needed(struct pcifs *fs, struct pcifs_vnode *base, const char *name)
{
	struct pcifs_vnode *v;

	v = pcifs_find_in_dir(base, name, 0, strlen(name));
	if(v == NULL) {
		v = pcifs_create_vnode(fs, name);
		if(v == NULL)
			return NULL;

		// set up the new node
		v->stream.name = kmalloc(strlen("") + 1);
		if(v->stream.name == NULL) {
			pcifs_delete_vnode(fs, v, true);
			return NULL;
		}
		strcpy(v->stream.name, "");
		v->stream.type = STREAM_TYPE_DIR;
		v->stream.u.dir.dir_head = NULL;

		// insert it into the parent dir
		pcifs_insert_in_dir(base, v);

		hash_insert(fs->vnode_list_hash, v);
	}		

	return v;
}

static int pcifs_create_vnode_tree(struct pcifs *fs, struct pcifs_vnode *base)
{
	int bus, unit, function;
	struct pci_cfg *cfg = NULL;
	struct pcifs_vnode *v;
	struct pcifs_vnode *new_vnode;

	dprintf("pcifs_create_vnode_tree: entry\n");

	for(bus = 0; bus < 256; bus++) {
		char bus_txt[4];
		sprintf(bus_txt, "%d", bus);
		for(unit = 0; unit < 32; unit++) {
			char unit_txt[3];
			sprintf(unit_txt, "%d", unit);
			for(function = 0; function < 8; function++) {
				char func_txt[2];
				sprintf(func_txt, "%d", function);

				if(cfg == NULL)
					cfg = kmalloc(sizeof(struct pci_cfg));
				if(pci_probe(bus, unit, function, cfg) < 0) {
					// not possible for a unit to have a hole in functions
					// if we dont find one in this unit, there are no more
					break;
				}

				v = pcifs_create_dir_if_needed(fs, base, bus_txt);
				v = pcifs_create_dir_if_needed(fs, v, unit_txt);
				v = pcifs_create_dir_if_needed(fs, v, func_txt);

				dprintf("created node %s/%s/%s/%s\n", bus_txt, unit_txt, func_txt, "ctrl");

				new_vnode = pcifs_create_vnode(fs, "ctrl");
				if(new_vnode == NULL)
					return ERR_NO_MEMORY;

				// set up the new node
				new_vnode->stream.name = kmalloc(strlen("") + 1);
				if(new_vnode->stream.name == NULL) {
					pcifs_delete_vnode(fs, new_vnode, true);
					return ERR_NO_MEMORY;
				}
				strcpy(new_vnode->stream.name, "");
				new_vnode->stream.type = STREAM_TYPE_DEVICE;
				new_vnode->stream.u.dev.cfg = cfg;
				cfg = NULL;

				// insert it into the parent dir
				pcifs_insert_in_dir(v, new_vnode);

				hash_insert(fs->vnode_list_hash, new_vnode);	
			}
		}
	}

	if(cfg != NULL)
		kfree(cfg);

	return 0;
}
	
static int pci_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *base = _base_vnode;
	struct pcifs_vnode *v;
	struct pcifs_cookie *cookie;
	struct pcifs_stream *s;
	int err = 0;
	int start = 0;
	
	dprintf("pci_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

	mutex_lock(&fs->lock);

	v = pcifs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
	if(v == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	if(redir->redir == true) {
		// loop back into the vfs because the parse hit a mount point
		redir->vnode = v->redir_vnode;
		redir->path = &path[start];
		err = 0;
		goto out;
	}

	s = pcifs_get_stream_from_vnode(v, stream, stream_type);
	if(s == NULL) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}

	cookie = kmalloc(sizeof(struct pcifs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	cookie->s = s;
	switch(stream_type) {
		case STREAM_TYPE_DIR:
			cookie->u.dir.ptr = s->u.dir.dir_head;
			break;
		case STREAM_TYPE_DEVICE:
			break;
		default:
			dprintf("pcifs_open: unhandled stream type\n");
	}

	*_cookie = cookie;
	*_vnode = v;	

	err = 0;

out:
err:
	mutex_unlock(&fs->lock);

	return err;
}

static int pci_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v = _vnode;
	struct pcifs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("pcifs_seek: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, seek_type %d\n", v, cookie, pos, seek_type);
	
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
		case STREAM_TYPE_DEVICE:
		default:
			err = ERR_INVALID_ARGS;
			break;
	}

	mutex_unlock(&fs->lock);
	return err;
}

static int pci_close(void *_fs, void *_vnode, void *_cookie)
{
	dprintf("pci_close: entry\n");

	if(_cookie)
		kfree(_cookie);

	return 0;
}

static int pci_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *base = _base_vnode;
	struct pcifs_vnode *v;
	int err;
	int start = 0;
	
	dprintf("pci_create: entry\n");

	mutex_lock(&fs->lock);

	v = pcifs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
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

	err = ERR_VFS_READONLY_FS;

err:	
	mutex_unlock(&fs->lock);
	return err;
}

static int pci_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *base = _base_vnode;
	struct pcifs_vnode *v;
	int err;
	int start = 0;
	
	dprintf("pci_stat: entry path '%s'\n", path);

	mutex_lock(&fs->lock);

	v = pcifs_get_vnode_from_path(fs, base, path, &start, &redir->redir);
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

	stat->size = 0;
	err = 0;

err:	
	mutex_unlock(&fs->lock);
	return err;	
}

static int pci_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v = _vnode;
	struct pcifs_cookie *cookie = _cookie;
	int err = 0;

	dprintf("pcifs_read: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x (0x%x)\n", v, cookie, pos, len, *len);

	mutex_lock(&fs->lock);
	
	switch(cookie->s->type) {
		case STREAM_TYPE_DIR: {
			dprintf("pcifs_read: cookie is type DIR\n");

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
		default:
			err = ERR_INVALID_ARGS;
	}
err:
	mutex_unlock(&fs->lock);

	return err;
}

static int pci_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	dprintf("pci_write: entry, len = %d\n", *len);

	*len = 0;
	return ERR_VFS_READONLY_FS;
}

static int pci_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v = _vnode;
	int err = 0;

	mutex_lock(&fs->lock);

	switch(op) {
		case PCI_GET_CFG:
			if(len < sizeof(struct pci_cfg)) {
				err= -1;
				goto err;
			}

			memcpy(buf, v->stream.u.dev.cfg, sizeof(struct pci_cfg));
			break;
		case PCI_DUMP_CFG:
			dump_pci_config(v->stream.u.dev.cfg);
			break;
		default:
			err = ERR_INVALID_ARGS;
			goto err;
	}

err:
	mutex_unlock(&fs->lock);
	return err;
}

static int pci_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct pcifs *fs;
	struct pcifs_vnode *v;
	int err;

	fs = kmalloc(sizeof(struct pcifs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->lock, "pci_mutex");
	if(err < 0) {
		goto err1;
	}

	fs->vnode_list_hash = hash_init(PCIFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		NULL, &pcifs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// create a vnode
	v = pcifs_create_vnode(fs, "");
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

	err = pcifs_create_vnode_tree(fs, fs->root_vnode);
	if(err < 0) {
		goto err4;
	}

	*root_vnode = v;
	*fs_cookie = fs;

	return 0;

err4:
	pcifs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:	
	kfree(fs);
err:
	return err;
 }

static int pci_unmount(void *_fs)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v;
	struct hash_iterator i;
	
	dprintf("pcifs_unmount: entry fs = 0x%x\n", fs);
	
	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct pcifs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		pcifs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);
	
	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	kfree(fs);

	return 0;
}

static int pci_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v = _v;
	
	v->redir_vnode = redir_vnode;
	
	return 0;
}

static int pci_unregister_mountpoint(void *_fs, void *_v)
{
	struct pcifs *fs = _fs;
	struct pcifs_vnode *v = _v;
	
	v->redir_vnode = NULL;
	
	return 0;
}

static int pci_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

struct fs_calls pci_hooks = {
	&pci_mount,
	&pci_unmount,
	&pci_register_mountpoint,
	&pci_unregister_mountpoint,
	&pci_dispose_vnode,
	&pci_open,
	&pci_seek,
	&pci_read,
	&pci_write,
	&pci_ioctl,
	&pci_close,
	&pci_create,
	&pci_stat,
};

int pci_bus_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("pci_bus_fs", &pci_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/bus", "", STREAM_TYPE_DIR);
	sys_create("/dev/bus/pci", "", STREAM_TYPE_DIR);
	sys_mount("/dev/bus/pci", "pci_bus_fs");

	bus_register_bus("/dev/bus/pci");

	return 0;
}

