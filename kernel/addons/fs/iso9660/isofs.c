/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/* Initial coding by Ithamar Adema, 2001 */

#include <kernel/kernel.h>
#include <kernel/vfs.h>

#include <kernel/khash.h>	// for vnode hash table handling
#include <kernel/heap.h>	// for kmalloc / kfree
#include <kernel/lock.h>	// for mutex handling
#include <kernel/vm.h>

#include <libc/string.h>

#include "isovol.h"

//================================================================================
// DEBUGGING FUNCTIONALITY
//================================================================================

#include <kernel/console.h>	// for kprintf
#include <kernel/debug.h>

#define ISOFS_TRACE 0

#if ISOFS_TRACE
#define TRACE(x) kprintf x
#else
#define TRACE(x)
#endif

//================================================================================
// PRIVATE STRUCTURES FUNCTIONALITY
//================================================================================

#define ISOFS_HASH_SIZE 16

// our private filesystem structure
struct isofs {
	int fd; // filedescriptor for ISO9660 file/dev
	fs_id id; // vfs filesystem id
	mutex lock; // lock
	int next_vnode_id; // next available vnode id
	void *vnode_list_hash; // hashtable of all used vnodes
	struct isofs_vnode *root_vnode; // pointer to private vnode struct of root dir of fs

	// ISO9660 stuff
	char volumename[33];// name of volume
	size_t blocksize;	// Size of an ISO sector
	long numblocks;		// Number of sectors on ISO image
};

//--------------------------------------------------------------------------------
struct isofs_cookie {
	struct isofs_stream *s;
	int oflags;
	union {
		struct {
			off_t pos;			// Position in directory/file data
		} file;
		struct {
			struct isofs_vnode* ptr;
		} dir;
	} u;
};

//--------------------------------------------------------------------------------
// our private stream structure
struct isofs_stream {
	stream_type type;	// is this stream a directory or file?
	off_t desc_pos;		// position in ISO of dir descriptor
	off_t data_pos;		// position in ISO of dir data
	size_t data_len;	// length of data
	struct isofs_vnode *dir_head;	// Pointer to first entry in directory
};

//--------------------------------------------------------------------------------
// our private vnode structure
struct isofs_vnode {
	struct isofs_vnode *all_next;	// next ptr of the global vnode linked-list (for hash)
	vnode_id id;					// our mapping back to vfs vnodes
	char *name;						// name of this vnode
	struct isofs_vnode *parent;		// vnode of parent (directory)
	struct isofs_vnode *dir_next;	// vnode of next entry in parent
	struct isofs_stream stream;		// private stream info
	bool scanned;					// true if dir has been scanned, false if not
};

static void iso_scan_dir(struct isofs* fs, struct isofs_vnode* dir);

//--------------------------------------------------------------------------------
static unsigned int isofs_vnode_hash_func(void *_v, void *_key, int range)
{
	struct isofs_vnode *v = _v;	// convenience ptr to our vnode structure
	vnode_id *key = _key;		// convenience ptr to vfs vnode ID

	if(v != NULL)				// If we've got a private vnode structure
		return v->id % range;	//   use that to calc hash value
	else						// else
		return (*key) % range;	//   use kernel vnode id
}

//--------------------------------------------------------------------------------
static int isofs_vnode_compare_func(void *_v, void *_key)
{
	struct isofs_vnode *v = _v;	// convenience ptr to our vnode structure
	vnode_id *key = _key;		// convenience ptr to vfs vnode ID

	if(v->id == *key)			// If the passed kernel vnode ID == private vnode ID
		return 0;				//    we're the same!
	else						// else
		return -1;				//    nope, not equal :(
}

//--------------------------------------------------------------------------------
static struct isofs_vnode *isofs_create_vnode(struct isofs *fs, const char *name)
{
	struct isofs_vnode *v;	// private vnode structure to be created

	// try to allocate our private vnode structure
	v = kmalloc(sizeof(struct isofs_vnode));
	if(v == NULL) // return NULL if not able to
		return NULL;

	// initialize our vnode structure
	memset(v, 0, sizeof(struct isofs_vnode));
	v->id = fs->next_vnode_id++;

	// copy name of vnode
	v->name = kmalloc(strlen(name) + 1);
	if(v->name == NULL) {
		kfree(v);
		return NULL;
	}
	strcpy(v->name, name);

	// return newly allocated private vnode structure
	return v;
}

//--------------------------------------------------------------------------------
static int isofs_delete_vnode(struct isofs *fs, struct isofs_vnode *v, bool force_delete)
{
	// cant delete it if it's in a directory or is a directory
	// and has children
//TODO:
//	if(!force_delete && ((v->stream.type == STREAM_TYPE_DIR && v->stream.u.dir.dir_head != NULL) || v->dir_next != NULL)) {
//		return ERR_NOT_ALLOWED;
//	}

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	// free name if any
	if(v->name != NULL)
		kfree(v->name);

	// free vnode structure itself
	kfree(v);

	// Tell caller all is ok
	return 0;
}

//--------------------------------------------------------------------------------
static int isofs_insert_in_dir(struct isofs_vnode *dir, struct isofs_vnode *v)
{
	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_INVALID_ARGS;

	v->dir_next = dir->stream.dir_head;
	dir->stream.dir_head = v;

	v->parent = dir;
	return 0;
}

//--------------------------------------------------------------------------------
static struct isofs_vnode* isofs_find_in_dir(struct isofs *fs, struct isofs_vnode* dir, const char* path)
{
	struct isofs_vnode *v;

	if(dir->stream.type != STREAM_TYPE_DIR)
		return NULL;

	iso_scan_dir(fs, dir);

	for(v = dir->stream.dir_head; v; v = v->dir_next) {
		if(strcmp(v->name, path) == 0) {
			return v;
		}
	}

	return NULL;
}

//================================================================================
// ISO9660 FORMAT HANDLING
//================================================================================
#define ISODIRENTRYBUF_SIZE	sizeof(iso_dir_entry)+32

static struct isofs_vnode* iso_add_entry(struct isofs* fs, struct isofs_vnode* parent,
										iso_dir_entry* e, off_t desc_pos)
{
	struct isofs_vnode* v;
	char name[33];

	// Create ASCIIZ name
	strncpy(name, e->name, e->nameLength);
	name[e->nameLength] = '\0';

	// Create initial vnode
	v = isofs_create_vnode(fs,name);

	// Fill in the stream member of the vnode
	v->stream.type = (e->flags & 2) ? STREAM_TYPE_DIR : STREAM_TYPE_FILE;
	v->stream.desc_pos = desc_pos;
	v->stream.data_pos = e->dataStartSector[ISO_LSB_INDEX] * fs->blocksize;
	v->stream.data_len = e->dataLength[ISO_LSB_INDEX];

	// Insert it into the hierarchy
	if (parent != NULL) {
		isofs_insert_in_dir(parent,v);
	} else {
		v->parent = v;
	}

	// Insert it into our hashing table
	hash_insert(fs->vnode_list_hash, v);

	return v;
}

//--------------------------------------------------------------------------------
static void iso_scan_dir(struct isofs* fs, struct isofs_vnode* dir)
{
	iso_volume_descriptor voldesc;
	char entryBuf[ISODIRENTRYBUF_SIZE];
	iso_dir_entry* e;
	off_t start, end;
	int cnt = 0;

	if (dir->scanned) {
		return;
	}

	start = dir->stream.data_pos;
	end = start + dir->stream.data_len;

	do
	{
		sys_read(fs->fd, entryBuf, start, ISODIRENTRYBUF_SIZE);
		e = (iso_dir_entry*)&entryBuf[0];

		if (e->recordLength == 0) {
			break;
		} else {
			// Check for . and .. entries and correct name
			if (cnt == 0 && e->nameLength <= 1 && e->name[0] < 32) {
				strcpy(e->name, ".");
				e->nameLength = 1;
			} else if (cnt == 1 && e->nameLength <= 1 && e->name[0] < 32) {
				strcpy(e->name, "..");
				e->nameLength = 2;
			}

			iso_add_entry(fs, dir, e, start);
			start += e->recordLength;
			++cnt;
		}
	} while( start < end );

	dir->scanned = true;
}

//--------------------------------------------------------------------------------
static void iso_init_volume(struct isofs* fs)
{
	iso_volume_descriptor voldesc;
	iso_dir_entry* e;

	// Read ISO9660 volume descriptor
	sys_read(fs->fd, &voldesc, ISO_VD_START, sizeof(voldesc));

	// Copy volume name
	strncpy(fs->volumename, voldesc.volumeID, 32);
	fs->volumename[32] = '\0';

	// Copy block size + # of blocks
	fs->blocksize = voldesc.sectorSize[ISO_LSB_INDEX];
	fs->numblocks = voldesc.numSectors[ISO_LSB_INDEX];

	// Get pointer to root directory entry
	e = (iso_dir_entry*)&voldesc.rootDirEntry[0];

	// Setup our root directory
	fs->root_vnode = iso_add_entry(fs, NULL, e,
						ISO_VD_START + voldesc.rootDirEntry - &voldesc.type);
}

//================================================================================
// VFS DRIVER API IMPLEMENTATION
//================================================================================

static int isofs_mount(fs_cookie *_fs, fs_id id, const char *device, void *args, vnode_id *root_vnid)
{
	struct isofs *fs;
	struct isofs_vnode *v;
	int err;

	TRACE(("isofs_mount: entry\n"));

	fs = kmalloc(sizeof(struct isofs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	fs->id = id;
	fs->next_vnode_id = 0;

	// Try to open the ISO file to read
	fs->fd = sys_open(device, STREAM_TYPE_ANY, 0);
	if (fs->fd < 0) {
		err = fs->fd;
		goto err1;
	}

	err = mutex_init(&fs->lock, "isofs_mutex");
	if(err < 0) {
		goto err1;
	}

	// Create and setup hash table
	fs->vnode_list_hash = hash_init(ISOFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		&isofs_vnode_compare_func, &isofs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// Read the ISO9660 info, and create root vnode
	iso_init_volume(fs);

	*root_vnid = fs->root_vnode->id;
	*_fs = fs;

	return 0;

err4:
	isofs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:
	kfree(fs);
err:
	return err;
}

//--------------------------------------------------------------------------------
static int isofs_unmount(fs_cookie _fs)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v;
	struct hash_iterator i;

	TRACE(("isofs_unmount: entry fs = 0x%x\n", fs));

	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct isofs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		isofs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);

	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	sys_close(fs->fd);
	kfree(fs);

	return 0;
}

//--------------------------------------------------------------------------------
static int isofs_sync(fs_cookie fs)
{
	TRACE(("isofs_sync: entry\n"));

	return 0;
}

//--------------------------------------------------------------------------------
static int isofs_lookup(fs_cookie _fs, fs_vnode _dir, const char *name, vnode_id *id)
{
	struct isofs *fs = (struct isofs *)_fs;
	struct isofs_vnode *dir = (struct isofs_vnode *)_dir;
	struct isofs_vnode *v;
	struct isofs_vnode *v1;
	int err;

	TRACE(("isofs_lookup: entry dir 0x%x, name '%s'\n", dir, name));

	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	mutex_lock(&fs->lock);

	// look it up
	v = isofs_find_in_dir(fs, dir, name);
	if(!v) {
		err = ERR_NOT_FOUND;
		goto err;
	}

	err = vfs_get_vnode(fs->id, v->id, (fs_vnode *)&v1);
	if(err < 0) {
		goto err;
	}

	*id = v->id;

	err = NO_ERROR;

err:
	mutex_unlock(&fs->lock);

	return err;
}

//--------------------------------------------------------------------------------
static int isofs_getvnode(fs_cookie _fs, vnode_id id, fs_vnode *v, bool r)
{
	struct isofs *fs = (struct isofs *)_fs;
	int err;

	TRACE(("isofs_getvnode: asking for vnode 0x%x 0x%x, r %d\n", id, r));

	if(!r)
		mutex_lock(&fs->lock);

	*v = hash_lookup(fs->vnode_list_hash, &id);

	if(!r)
		mutex_unlock(&fs->lock);

	TRACE(("isofs_getnvnode: looked it up at 0x%x\n", *v));

	if(*v)
		return 0;
	else
		return ERR_NOT_FOUND;
}

//--------------------------------------------------------------------------------
static int isofs_putvnode(fs_cookie _fs, fs_vnode _v, bool r)
{
	struct isofs_vnode *v = (struct isofs_vnode *)_v;

	TRACE(("isofs_putvnode: entry on vnode 0x%x 0x%x, r %d\n", v->id, r));

	return 0; // whatever
}

static int isofs_removevnode(fs_cookie _fs, fs_vnode _v, bool r)
{
	struct isofs *fs = (struct isofs *)_fs;
	struct isofs_vnode *v = (struct isofs_vnode *)_v;
	struct isofs_vnode dummy;
	int err;

	TRACE(("isofs_removevnode: remove 0x%x (0x%x 0x%x), r %d\n", v, v->id, r));

	if(!r)
		mutex_lock(&fs->lock);

	if(v->dir_next) {
		// can't remove node if it's linked to the dir
		panic("isofs_removevnode: vnode %p asked to be removed is present in dir\n", v);
	}

	isofs_delete_vnode(fs, v, false);

	err = 0;

err:
	if(!r)
		mutex_unlock(&fs->lock);

	return err;
}

//--------------------------------------------------------------------------------
static int isofs_fsync(fs_cookie _fs, fs_vnode _v)
{
	struct isofs_vnode *v = (struct isofs_vnode *)_v;
	TRACE(("isofs_fsync: entry on vnode 0x%x\n", v->id));
	return 0;
}

//--------------------------------------------------------------------------------
static int isofs_open(fs_cookie _fs, fs_vnode _v, file_cookie *_cookie, stream_type st, int oflags)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	struct isofs_cookie *cookie;
	int err = 0;
	int start = 0;

	TRACE(("isofs_open: vnode 0x%x, stream_type %d, oflags 0x%x\n", v, st, oflags));

	// Check stream type
	if(st != STREAM_TYPE_ANY && st != v->stream.type) {
		err = ERR_VFS_WRONG_STREAM_TYPE;
		goto err;
	}

	// Allocate our cookie for storage of position
	cookie = kmalloc(sizeof(struct isofs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	mutex_lock(&fs->lock);

	// Setup our cookie to start of stream data
	cookie->s = &v->stream;
	switch(v->stream.type) {
		case STREAM_TYPE_DIR:
			iso_scan_dir(fs, v);
			cookie->u.dir.ptr = v->stream.dir_head;
			break;

		case STREAM_TYPE_FILE:
			cookie->u.file.pos = 0;
			break;

		default:
			err = ERR_VFS_WRONG_STREAM_TYPE;
			kfree(cookie);
			cookie = NULL;
	}

	// Give cookie to caller
	*_cookie = cookie;

err1:
	mutex_unlock(&fs->lock);
err:
	return err;
}

//--------------------------------------------------------------------------------
static int isofs_close(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	struct isofs_cookie *cookie = _cookie;

	TRACE(("isofs_close: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	return 0;
}

//--------------------------------------------------------------------------------
static int isofs_freecookie(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	struct isofs_cookie *cookie = _cookie;

	TRACE(("isofs_freecookie: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	if (cookie)
		kfree(cookie);

	return 0;
}

//--------------------------------------------------------------------------------
static ssize_t isofs_read(fs_cookie _fs, fs_vnode _v, file_cookie _cookie,
							void *buf, off_t pos, ssize_t len)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	struct isofs_cookie *cookie = _cookie;
	ssize_t err = 0;
	char tempbuf[len];

	TRACE(("isofs_read: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x\n", v, cookie, pos, len));

	mutex_lock(&fs->lock);

	switch(cookie->s->type) {
		case STREAM_TYPE_DIR: {
			// If at end of entries, bail out
			if(cookie->u.dir.ptr == NULL) {
				err = 0;
				break;
			}

			// If buffer is too small, bail out
			if ((ssize_t)strlen(cookie->u.dir.ptr->name) + 1 > len) {
				err = ERR_VFS_INSUFFICIENT_BUF;
				goto error;
			}

			// Copy name to buffer
			err = user_strcpy(buf, cookie->u.dir.ptr->name);
			if(err < 0)
				goto error;

			// Return length of name
			err = strlen(cookie->u.dir.ptr->name) + 1;

			// Move to next entry
			cookie->u.dir.ptr = cookie->u.dir.ptr->dir_next;
			break;
		}

		case STREAM_TYPE_FILE:
			// If size is negative, forget it
			if(len <= 0) {
				err = 0;
				break;
			}

			// If position is negative, we'll read from current pos
			if (pos < 0) {
				// we'll read where the cookie is at
				pos = cookie->u.file.pos;
			}


			// If position is past filelength, forget it
			if (pos >= cookie->s->data_len) {
				err = 0;
				break;
			}

			// If read goes partially beyond EOF
			if (pos + len > cookie->s->data_len) {
				// trim the read
				len = cookie->s->data_len - pos;
			}

			err = sys_read(fs->fd, tempbuf, cookie->s->data_pos + pos, len);
			if(err < 0) {
				goto error;
			}
			user_memcpy(buf, tempbuf, err);

			// Move to next bit of the file
			cookie->u.file.pos = pos + err;
			break;

		default:
			err = ERR_INVALID_ARGS;
	}
error:
	mutex_unlock(&fs->lock);

	return err;
}

//--------------------------------------------------------------------------------
static ssize_t isofs_write(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	TRACE(("isofs_write: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, len 0x%x\n", v, cookie, pos, len));

	return ERR_VFS_READONLY_FS;
}

//--------------------------------------------------------------------------------
static int isofs_seek(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, off_t pos, seek_type st)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	struct isofs_cookie *cookie = _cookie;
	int err = 0;

	TRACE(("isofs_seek: vnode 0x%x, cookie 0x%x, pos 0x%x 0x%x, seek_type %d\n", v, cookie, pos, st));

	mutex_lock(&fs->lock);

	switch(cookie->s->type) {
		case STREAM_TYPE_DIR:
			switch(st) {
				// only valid args are seek_type SEEK_SET, pos 0.
				// this rewinds to beginning of directory
				case SEEK_SET:
					if(pos == 0) {
						cookie->u.dir.ptr = cookie->s->dir_head;
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
			switch(st) {
				case SEEK_SET:
					if(pos < 0)
						pos = 0;
					if(pos > cookie->s->data_len)
						pos = cookie->s->data_len;
					cookie->u.file.pos = pos;
					break;
				case SEEK_CUR:
					if(pos + cookie->u.file.pos > cookie->s->data_len)
						cookie->u.file.pos = cookie->s->data_len;
					else if(pos + cookie->u.file.pos < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos += pos;
					break;
				case SEEK_END:
					if(pos > 0)
						cookie->u.file.pos = cookie->s->data_len;
					else if(pos + cookie->s->data_len < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos = pos + cookie->s->data_len;
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

//--------------------------------------------------------------------------------
static int isofs_ioctl(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, int op, void *buf, size_t len)
{
	TRACE(("isofs_ioctl: vnode 0x%x, cookie 0x%x, op %d, buf 0x%x, len 0x%x\n", _v, _cookie, op, buf, len));

	return ERR_INVALID_ARGS;
}

//--------------------------------------------------------------------------------
static int isofs_canpage(fs_cookie _fs, fs_vnode _v)
{
	struct isofs_vnode *v = _v;

	TRACE(("isofs_canpage: vnode 0x%x\n", v));

	return 0;
}

//--------------------------------------------------------------------------------
static ssize_t isofs_readpage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;

	TRACE(("isofs_readpage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos));

	return ERR_NOT_ALLOWED;
}

//--------------------------------------------------------------------------------
static int isofs_create(fs_cookie _fs, fs_vnode _dir, const char *name, stream_type st, void *create_args, vnode_id *new_vnid)
{
	return ERR_VFS_READONLY_FS;
}

//--------------------------------------------------------------------------------
static int isofs_unlink(fs_cookie _fs, fs_vnode _dir, const char *name)
{
	return ERR_VFS_READONLY_FS;
}

//--------------------------------------------------------------------------------
static int isofs_rename(fs_cookie _fs, fs_vnode _olddir, const char *oldname, fs_vnode _newdir, const char *newname)
{
	return ERR_VFS_READONLY_FS;
}

//--------------------------------------------------------------------------------
static ssize_t isofs_writepage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;

	TRACE(("isofs_writepage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos));

	return ERR_NOT_ALLOWED;
}

//--------------------------------------------------------------------------------
static int isofs_rstat(fs_cookie _fs, fs_vnode _v, struct file_stat *stat)
{
	struct isofs *fs = _fs;
	struct isofs_vnode *v = _v;
	int err = 0;

	TRACE(("isofs_rstat: vnode 0x%x (0x%x 0x%x), stat 0x%x\n", v, v->id, stat));

	mutex_lock(&fs->lock);

	stat->vnid = v->id;
	stat->type = v->stream.type;

	switch(v->stream.type) {
		case STREAM_TYPE_DIR:
			stat->size = 0;
			break;
		case STREAM_TYPE_FILE:
			stat->size = v->stream.data_len;
			break;
		default:
			err = ERR_INVALID_ARGS;
			break;
	}

err:
	mutex_unlock(&fs->lock);

	return err;
}

//--------------------------------------------------------------------------------
static int isofs_wstat(fs_cookie _fs, fs_vnode _v, struct file_stat *stat, int stat_mask)
{
	return ERR_VFS_READONLY_FS;
}

//================================================================================
// REGISTRATION HANDLING
//================================================================================

static struct fs_calls isofs_calls = {
	&isofs_mount,		// mount
	&isofs_unmount,		// unmount
	&isofs_sync,		// sync

	isofs_lookup,		// lookup

	&isofs_getvnode,	// getvnode
	&isofs_putvnode,	// putvnode
	&isofs_removevnode,	// removevnode

	&isofs_open,		// open
	&isofs_close,		// close
	&isofs_freecookie,	// freecookie
	&isofs_fsync,		// fsync

	&isofs_read,		// read
	&isofs_write,		// write
	&isofs_seek,		// seek
	&isofs_ioctl,		// ioctl

	&isofs_canpage,		// canpage
	&isofs_readpage,	// readpage
	&isofs_writepage,	// writepage

	&isofs_create,		// create
	&isofs_unlink,		// unlink
	&isofs_rename,		// rename

	&isofs_rstat,		// rstat
	&isofs_wstat		// wstat
};

int fs_bootstrap(void);

//--------------------------------------------------------------------------------
int fs_bootstrap(void)
{
	dprintf("bootstrap_isofs: entry\n");
	return vfs_register_filesystem("isofs", &isofs_calls);
}
