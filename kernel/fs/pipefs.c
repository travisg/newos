/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/sem.h>
#include <kernel/thread.h>
#include <newos/errors.h>
#include <newos/drivers.h>
#include <newos/pipefs_priv.h>

#include <kernel/arch/cpu.h>

#include <kernel/fs/pipefs.h>

#include <string.h>
#include <stdio.h>


#define PIPEFS_TRACE 0

#if PIPEFS_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

#define PIPE_BUFFER_LEN 256

#define PIPE_FLAGS_ANONYMOUS 1

struct pipefs_stream {
	stream_type type;
	union {
		struct stream_dir {
			struct pipefs_vnode *dir_head;
			struct pipefs_cookie *jar_head;
			mutex dir_lock;
		} dir;
		struct stream_pipe {
			int flags;
			int read_count;
			int write_count;
			int open_count;

			mutex lock;
			sem_id write_sem;
			sem_id read_sem;

			char *buf;
			int buf_len;
			int head;
			int tail;
		} pipe;
	} u;
};

struct pipefs_vnode {
	struct pipefs_vnode *all_next;
	vnode_id id;
	char *name;
	struct pipefs_vnode *parent;
	struct pipefs_vnode *dir_next;
	struct pipefs_stream stream;
};

struct pipefs {
	fs_id id;
	int next_vnode_id;
	mutex hash_lock;
	void *vnode_list_hash;
	struct pipefs_vnode *root_vnode;
	struct pipefs_vnode *anon_vnode;
};

struct pipefs_cookie {
	struct pipefs_stream *s;
	int oflags;
	union {
		struct cookie_dir {
			struct pipefs_cookie *next;
			struct pipefs_cookie *prev;
			struct pipefs_vnode *ptr;
		} dir;
		struct cookie_pipe {
		} pipe;
	} u;
};

/* the one and only allowed pipefs instance */
static struct pipefs *thepipefs = NULL;

#define PIPEFS_HASH_SIZE 16
static unsigned int pipefs_vnode_hash_func(void *_v, const void *_key, unsigned int range)
{
	struct pipefs_vnode *v = _v;
	const vnode_id *key = _key;

	if(v != NULL)
		return v->id % range;
	else
		return (*key) % range;
}

static int pipefs_vnode_compare_func(void *_v, const void *_key)
{
	struct pipefs_vnode *v = _v;
	const vnode_id *key = _key;

	if(v->id == *key)
		return 0;
	else
		return -1;
}

static struct pipefs_vnode *pipefs_create_vnode(struct pipefs *fs, const char *name, stream_type type)
{
	struct pipefs_vnode *v;

	v = kmalloc(sizeof(struct pipefs_vnode));
	if(v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct pipefs_vnode));
	v->id = atomic_add(&fs->next_vnode_id, 1);

	v->name = kstrdup(name);
	if(v->name == NULL)
		goto err;

	v->stream.type = type;
	switch(type) {
		case STREAM_TYPE_DIR:
			v->stream.u.dir.dir_head = NULL;
			v->stream.u.dir.jar_head = NULL;
			if(mutex_init(&v->stream.u.dir.dir_lock, "pipefs_dir_lock") < 0)
				goto err;
			break;
		case STREAM_TYPE_PIPE:
			v->stream.u.pipe.buf = kmalloc(PIPE_BUFFER_LEN);
			if(v->stream.u.pipe.buf == NULL)
				goto err;
			v->stream.u.pipe.buf_len = PIPE_BUFFER_LEN;

			if(mutex_init(&v->stream.u.pipe.lock, "pipe_lock") < 0) {
				kfree(v->stream.u.pipe.buf);
				goto err;
			}
			v->stream.u.pipe.read_sem = sem_create(0, "pipe_read_sem");
			if(v->stream.u.pipe.read_sem < 0) {
				mutex_destroy(&v->stream.u.pipe.lock);
				kfree(v->stream.u.pipe.buf);
				goto err;
			}
			v->stream.u.pipe.write_sem = sem_create(1, "pipe_write_sem");
			if(v->stream.u.pipe.write_sem < 0) {
				sem_delete(v->stream.u.pipe.read_sem);
				mutex_destroy(&v->stream.u.pipe.lock);
				kfree(v->stream.u.pipe.buf);
				goto err;
			}
			break;
	}

	return v;

err:
	if(v->name)
		kfree(v->name);
	kfree(v);
	return NULL;
}

static int pipefs_delete_vnode(struct pipefs *fs, struct pipefs_vnode *v, bool force_delete)
{
	// cant delete it if it's in a directory or is a directory
	// and has children
	if(!force_delete && ((v->stream.type == STREAM_TYPE_DIR && v->stream.u.dir.dir_head != NULL) || v->dir_next != NULL)) {
		return ERR_NOT_ALLOWED;
	}

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	if(v->stream.type == STREAM_TYPE_PIPE) {
		sem_delete(v->stream.u.pipe.write_sem);
		sem_delete(v->stream.u.pipe.read_sem);
		mutex_destroy(&v->stream.u.pipe.lock);
		kfree(v->stream.u.pipe.buf);
	}

	if(v->name != NULL)
		kfree(v->name);
	kfree(v);

	return 0;
}

static void insert_cookie_in_jar(struct pipefs_vnode *dir, struct pipefs_cookie *cookie)
{
	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	cookie->u.dir.next = dir->stream.u.dir.jar_head;
	dir->stream.u.dir.jar_head = cookie;
	cookie->u.dir.prev = NULL;
}

static void remove_cookie_from_jar(struct pipefs_vnode *dir, struct pipefs_cookie *cookie)
{
	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	if(cookie->u.dir.next)
		cookie->u.dir.next->u.dir.prev = cookie->u.dir.prev;
	if(cookie->u.dir.prev)
		cookie->u.dir.prev->u.dir.next = cookie->u.dir.next;
	if(dir->stream.u.dir.jar_head == cookie)
		dir->stream.u.dir.jar_head = cookie->u.dir.next;

	cookie->u.dir.prev = cookie->u.dir.next = NULL;
}

/* makes sure none of the dircookies point to the vnode passed in */
static void update_dircookies(struct pipefs_vnode *dir, struct pipefs_vnode *v)
{
	struct pipefs_cookie *cookie;

	for(cookie = dir->stream.u.dir.jar_head; cookie; cookie = cookie->u.dir.next) {
		if(cookie->u.dir.ptr == v) {
			cookie->u.dir.ptr = v->dir_next;
		}
	}
}


static struct pipefs_vnode *pipefs_find_in_dir(struct pipefs_vnode *dir, const char *path)
{
	struct pipefs_vnode *v;

	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	if(!strcmp(path, "."))
		return dir;
	if(!strcmp(path, ".."))
		return dir->parent;

	for(v = dir->stream.u.dir.dir_head; v; v = v->dir_next) {
		if(strcmp(v->name, path) == 0) {
			return v;
		}
	}
	return NULL;
}

static int pipefs_insert_in_dir(struct pipefs_vnode *dir, struct pipefs_vnode *v)
{
	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	v->dir_next = dir->stream.u.dir.dir_head;
	dir->stream.u.dir.dir_head = v;

	v->parent = dir;
	return 0;
}

static int pipefs_remove_from_dir(struct pipefs_vnode *dir, struct pipefs_vnode *findit)
{
	struct pipefs_vnode *v;
	struct pipefs_vnode *last_v;

	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	for(v = dir->stream.u.dir.dir_head, last_v = NULL; v; last_v = v, v = v->dir_next) {
		if(v == findit) {
			/* make sure all dircookies dont point to this vnode */
			update_dircookies(dir, v);

			if(last_v)
				last_v->dir_next = v->dir_next;
			else
				dir->stream.u.dir.dir_head = v->dir_next;
			v->dir_next = NULL;
			return 0;
		}
	}
	return -1;
}

#if 0
static int pipefs_is_dir_empty(struct pipefs_vnode *dir)
{
	ASSERT(dir->stream.type == STREAM_TYPE_DIR);
	ASSERT_LOCKED_MUTEX(&dir->stream.u.dir.dir_lock);

	return !dir->stream.u.dir.dir_head;
}
#endif

static int pipefs_mount(fs_cookie *_fs, fs_id id, const char *pipefs, void *args, vnode_id *root_vnid)
{
	struct pipefs *fs;
	struct pipefs_vnode *v;
	struct pipefs_vnode *anon_v;
	int err;

	TRACE(("pipefs_mount: entry\n"));

	if(thepipefs) {
		dprintf("double mount of pipefs attempted\n");
		err = ERR_GENERAL;
		goto err;
	}

	fs = kmalloc(sizeof(struct pipefs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->hash_lock, "pipefs_mutex");
	if(err < 0) {
		goto err1;
	}

	fs->vnode_list_hash = hash_init(PIPEFS_HASH_SIZE, (addr_t)&v->all_next - (addr_t)v,
		&pipefs_vnode_compare_func, &pipefs_vnode_hash_func);
	if(fs->vnode_list_hash == NULL) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	// create a vnode
	v = pipefs_create_vnode(fs, "", STREAM_TYPE_DIR);
	if(v == NULL) {
		err = ERR_NO_MEMORY;
		goto err3;
	}

	// set it up
	v->parent = v;

	fs->root_vnode = v;

	hash_insert(fs->vnode_list_hash, v);

	*root_vnid = v->id;
	*_fs = fs;

	// create the anonymous pipe
	anon_v = pipefs_create_vnode(fs, "anon", STREAM_TYPE_PIPE);
	if(anon_v == NULL) {
		err = ERR_NO_MEMORY;
		goto err4;
	}

	// set it up
	anon_v->parent = v;
	fs->anon_vnode = anon_v;

	hash_insert(fs->vnode_list_hash, anon_v);
	mutex_lock(&v->stream.u.dir.dir_lock);
	pipefs_insert_in_dir(v, anon_v);
	mutex_unlock(&v->stream.u.dir.dir_lock);

	thepipefs = fs;

	return 0;

err4:
	pipefs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->hash_lock);
err1:
	kfree(fs);
err:
	return err;
}

static int pipefs_unmount(fs_cookie _fs)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v;
	struct hash_iterator i;

	TRACE(("pipefs_unmount: entry fs = 0x%x\n", fs));

	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct pipefs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		pipefs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);

	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->hash_lock);
	kfree(fs);

	return 0;
}

static int pipefs_sync(fs_cookie fs)
{
	TRACE(("pipefs_sync: entry\n"));

	return 0;
}

static int pipefs_lookup(fs_cookie _fs, fs_vnode _dir, const char *name, vnode_id *id)
{
	struct pipefs *fs = (struct pipefs *)_fs;
	struct pipefs_vnode *dir = (struct pipefs_vnode *)_dir;
	struct pipefs_vnode *v;
	struct pipefs_vnode *v1;
	int err;

	TRACE(("pipefs_lookup: entry dir 0x%x, name '%s'\n", dir, name));

	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;


	// look it up
	mutex_lock(&dir->stream.u.dir.dir_lock);
	v = pipefs_find_in_dir(dir, name);
	mutex_unlock(&dir->stream.u.dir.dir_lock);
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
	return err;
}

static int pipefs_getvnode(fs_cookie _fs, vnode_id id, fs_vnode *v, bool r)
{
	struct pipefs *fs = (struct pipefs *)_fs;

	TRACE(("pipefs_getvnode: asking for vnode 0x%Lx, r %d\n", id, r));

	if(!r)
		mutex_lock(&fs->hash_lock);

	*v = hash_lookup(fs->vnode_list_hash, &id);

	if(!r)
		mutex_unlock(&fs->hash_lock);

	TRACE(("pipefs_getvnode: looked it up at 0x%x\n", *v));

	if(*v)
		return 0;
	else
		return ERR_NOT_FOUND;
}

static int pipefs_putvnode(fs_cookie _fs, fs_vnode _v, bool r)
{
	struct pipefs_vnode *v = (struct pipefs_vnode *)_v;

	TOUCH(v);

	TRACE(("pipefs_putvnode: entry on vnode 0x%Lx, r %d\n", v->id, r));

	return 0; // whatever
}

static int pipefs_removevnode(fs_cookie _fs, fs_vnode _v, bool r)
{
	struct pipefs *fs = (struct pipefs *)_fs;
	struct pipefs_vnode *v = (struct pipefs_vnode *)_v;
	int err;

	TRACE(("pipefs_removevnode: remove 0x%x (0x%Lx), r %d\n", v, v->id, r));

	if(!r)
		mutex_lock(&fs->hash_lock);

	if(v->dir_next) {
		// can't remove node if it's linked to the dir
		panic("pipefs_removevnode: vnode %p asked to be removed is present in dir\n", v);
	}

	pipefs_delete_vnode(fs, v, false);

	err = 0;

	if(!r)
		mutex_unlock(&fs->hash_lock);

	return err;
}

static int pipefs_opendir(fs_cookie _fs, fs_vnode _v, dir_cookie *_cookie)
{
	struct pipefs *fs = (struct pipefs *)_fs;
	struct pipefs_vnode *v = (struct pipefs_vnode *)_v;
	struct pipefs_cookie *cookie;
	int err = 0;

	TOUCH(fs);

	TRACE(("pipefs_opendir: vnode 0x%x\n", v));

	if(v->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	cookie = kmalloc(sizeof(struct pipefs_cookie));
	if(cookie == NULL)
		return ERR_NO_MEMORY;

	mutex_lock(&v->stream.u.dir.dir_lock);

	cookie->s = &v->stream;
	cookie->u.dir.ptr = v->stream.u.dir.dir_head;

	insert_cookie_in_jar(v, cookie);

	*_cookie = cookie;

	mutex_unlock(&v->stream.u.dir.dir_lock);
	return err;
}

static int pipefs_closedir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;

	TOUCH(fs);TOUCH(v);TOUCH(cookie);

	TRACE(("pipefs_closedir: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(v->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	mutex_lock(&v->stream.u.dir.dir_lock);

	if(cookie) {
		remove_cookie_from_jar(v, cookie);
		kfree(cookie);
	}

	mutex_unlock(&v->stream.u.dir.dir_lock);

	return 0;
}

static int pipefs_rewinddir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;

	TOUCH(v);TOUCH(fs);

	TRACE(("pipefs_rewinddir: vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(v->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	mutex_lock(&v->stream.u.dir.dir_lock);

	cookie->u.dir.ptr = cookie->s->u.dir.dir_head;

	mutex_unlock(&v->stream.u.dir.dir_lock);

	return err;
}

static int pipefs_readdir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie, void *buf, size_t len)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;

	TOUCH(fs);TOUCH(v);

	TRACE(("pipefs_readdir: vnode 0x%x, cookie 0x%x, len 0x%x\n", v, cookie, len));

	if(v->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	mutex_lock(&v->stream.u.dir.dir_lock);

	if(cookie->u.dir.ptr == NULL) {
		err = 0;
		goto err;
	}

	if(strlen(cookie->u.dir.ptr->name) + 1 > len) {
		err = ERR_VFS_INSUFFICIENT_BUF;
		goto err;
	}

	err = user_strcpy(buf, cookie->u.dir.ptr->name);
	if(err < 0)
		goto err;

	err = strlen(cookie->u.dir.ptr->name) + 1;

	cookie->u.dir.ptr = cookie->u.dir.ptr->dir_next;

err:
	mutex_unlock(&v->stream.u.dir.dir_lock);

	return err;
}

static int pipefs_open(fs_cookie _fs, fs_vnode _v, file_cookie *_cookie, stream_type st, int oflags)
{
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie;
	int err = 0;

	TRACE(("pipefs_open: vnode 0x%x, oflags 0x%x\n", v, oflags));

	if(v->stream.type == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	cookie = kmalloc(sizeof(struct pipefs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	cookie->s = &v->stream;

	mutex_lock(&v->stream.u.pipe.lock);
	// XXX right now always RDWR
	v->stream.u.pipe.read_count++;
	v->stream.u.pipe.write_count++;
	v->stream.u.pipe.open_count++;
	mutex_unlock(&v->stream.u.pipe.lock);

	*_cookie = cookie;

err:
	return err;
}

static int pipefs_close(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;

	TRACE(("pipefs_close: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(v->stream.type == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	ASSERT(cookie->s == &v->stream);

	mutex_lock(&v->stream.u.pipe.lock);
	// XXX right now always RDWR
	v->stream.u.pipe.read_count--;
	v->stream.u.pipe.write_count--;
	v->stream.u.pipe.open_count--;

	if(v->stream.u.pipe.flags & PIPE_FLAGS_ANONYMOUS) {
		if(v->stream.u.pipe.open_count < 2) {
			// we just closed the other side of the pipe, delete the writer sem
			// this will wake up any writers
			sem_delete(v->stream.u.pipe.write_sem);
			v->stream.u.pipe.write_sem = -1;
			sem_delete(v->stream.u.pipe.read_sem);
			v->stream.u.pipe.read_sem = -1;
		}
	}
	mutex_unlock(&v->stream.u.pipe.lock);

	return err;
}

static int pipefs_freecookie(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;

	TRACE(("pipefs_freecookie: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(cookie->s->type == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	ASSERT(cookie->s == &v->stream);

	if(cookie)
		kfree(cookie);

	return 0;
}

static int pipefs_fsync(fs_cookie _fs, fs_vnode _v)
{
	return 0;
}

static ssize_t pipefs_read(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	ssize_t err = 0;
	ssize_t read_len = 0;
	ssize_t data_len;

	TRACE(("pipefs_read: vnode 0x%x, cookie 0x%x, pos 0x%Lx, len 0x%x\n", v, cookie, pos, len));

	if(v->stream.type == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	ASSERT(cookie->s == &v->stream);

	if(v == fs->anon_vnode) {
		err = ERR_NOT_ALLOWED;
		goto err;
	}
	if(len < 0) {
		err = ERR_INVALID_ARGS;
		goto err;
	}
	if(len == 0) {
		err = 0;
		goto err;
	}

	// wait for data in the buffer
	err = sem_acquire_etc(v->stream.u.pipe.read_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
	if(err == ERR_SEM_INTERRUPTED)
		goto err;

	mutex_lock(&v->stream.u.pipe.lock);

	// see if the other endpoint is active
	if(v->stream.u.pipe.flags & PIPE_FLAGS_ANONYMOUS) {
		// this is an anonymous pipe, check the overall open count
		// and make sure it's >1, otherwise we're the only one holding it open
		if(v->stream.u.pipe.open_count < 2) {
			err = ERR_PIPE_WIDOW;
			goto done_pipe;
		}
	}

	ASSERT(v->stream.u.pipe.head < v->stream.u.pipe.buf_len);
	ASSERT(v->stream.u.pipe.tail < v->stream.u.pipe.buf_len);

	// read data
	if(v->stream.u.pipe.head >= v->stream.u.pipe.tail)
		data_len = v->stream.u.pipe.head - v->stream.u.pipe.tail;
	else
		data_len = (v->stream.u.pipe.head + v->stream.u.pipe.buf_len) - v->stream.u.pipe.tail;
	len = min(data_len, len);

	ASSERT(len > 0);

	while(len > 0) {
		ssize_t copy_len = len;

		if(v->stream.u.pipe.tail + copy_len > v->stream.u.pipe.buf_len) {
			copy_len = v->stream.u.pipe.buf_len - v->stream.u.pipe.tail;
		}

		err = user_memcpy((char *)buf + read_len, v->stream.u.pipe.buf + v->stream.u.pipe.tail, copy_len);
		if(err < 0) {
			sem_release(v->stream.u.pipe.read_sem, 1);
			goto done_pipe;
		}

		// update the buffer pointers
		v->stream.u.pipe.tail += copy_len;
		if(v->stream.u.pipe.tail == v->stream.u.pipe.buf_len)
			v->stream.u.pipe.tail = 0;
		len -= copy_len;
		read_len += copy_len;
	}

	// is there more data available?
	if(read_len < data_len)
		sem_release(v->stream.u.pipe.read_sem, 1);

	// did it used to be full?
	if(data_len == v->stream.u.pipe.buf_len - 1)
		sem_release(v->stream.u.pipe.write_sem, 1);

	err = read_len;

done_pipe:
	mutex_unlock(&v->stream.u.pipe.lock);
err:
	return err;
}

static ssize_t pipefs_write(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;
	ssize_t written = 0;
	ssize_t free_space;

	TRACE(("pipefs_write: vnode 0x%x, cookie 0x%x, pos 0x%Lx, len 0x%x\n", v, cookie, pos, len));

	if(v->stream.type == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	ASSERT(cookie->s == &v->stream);

	if(v == fs->anon_vnode) {
		err = ERR_NOT_ALLOWED;
		goto err;
	}
	if(len < 0) {
		err = ERR_INVALID_ARGS;
		goto err;
	}
	if(len == 0) {
		err = 0;
		goto err;
	}

	// wait on space in the circular buffer
	err = sem_acquire_etc(v->stream.u.pipe.write_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
	if(err == ERR_SEM_INTERRUPTED)
		goto err;

	mutex_lock(&v->stream.u.pipe.lock);

	// see if the other endpoint is active
	if(v->stream.u.pipe.flags & PIPE_FLAGS_ANONYMOUS) {
		// this is an anonymous pipe, check the overall open count
		// and make sure it's >1, otherwise we're the only one holding it open
		if(v->stream.u.pipe.open_count < 2) {
			// XXX deliver real SIGPIPE when we get it
			proc_kill_proc(proc_get_current_proc_id());
			err = ERR_PIPE_WIDOW;
			goto done_pipe;
		}
	}

	ASSERT(v->stream.u.pipe.head < v->stream.u.pipe.buf_len);
	ASSERT(v->stream.u.pipe.tail < v->stream.u.pipe.buf_len);

	// figure out how much data we can write to it
	if(v->stream.u.pipe.head >= v->stream.u.pipe.tail)
		free_space = v->stream.u.pipe.buf_len - (v->stream.u.pipe.head - v->stream.u.pipe.tail);
	else
		free_space = v->stream.u.pipe.buf_len - ((v->stream.u.pipe.head + v->stream.u.pipe.buf_len) - v->stream.u.pipe.tail);
	free_space = min(free_space, v->stream.u.pipe.buf_len - 1); // can't completely fill it
	len = min(free_space, len);

#if PIPEFS_TRACE
	dprintf("pipefs_write: buffer free space %d, len to write %d, head %d, tail %d\n", free_space, len, v->stream.u.pipe.head, v->stream.u.pipe.tail);
#endif

	ASSERT(len > 0);

	while(len > 0) {
		ssize_t copy_len = len;

		if(v->stream.u.pipe.head + copy_len > v->stream.u.pipe.buf_len) {
			copy_len = v->stream.u.pipe.buf_len - v->stream.u.pipe.head;
		}

		err = user_memcpy(v->stream.u.pipe.buf + v->stream.u.pipe.head, (char *)buf + written, copy_len);
		if(err < 0) {
			sem_release(v->stream.u.pipe.write_sem, 1);
			goto done_pipe;
		}

		// update the buffer pointers
		v->stream.u.pipe.head += copy_len;
		if(v->stream.u.pipe.head == v->stream.u.pipe.buf_len)
			v->stream.u.pipe.head = 0;
		len -= copy_len;
		written += copy_len;
	}

	// is there more space available?
	if(written < free_space)
		sem_release(v->stream.u.pipe.write_sem, 1);

	// did it used to be empty?
	if(free_space == v->stream.u.pipe.buf_len - 1)
		sem_release(v->stream.u.pipe.read_sem, 1);

	err = written;

done_pipe:
	mutex_unlock(&v->stream.u.pipe.lock);

err:
	return err;
}

static int pipefs_seek(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, off_t pos, seek_type st)
{
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;

	TRACE(("pipefs_seek: vnode 0x%x, cookie 0x%x, pos 0x%Lx, seek_type %d\n", v, cookie, pos, st));

	ASSERT(cookie->s == &v->stream);

	switch(cookie->s->type) {
		case STREAM_TYPE_DIR:
			mutex_lock(&v->stream.u.dir.dir_lock);
			switch(st) {
				// only valid args are seek_type _SEEK_SET, pos 0.
				// this rewinds to beginning of directory
				case _SEEK_SET:
					if(pos == 0) {
						cookie->u.dir.ptr = cookie->s->u.dir.dir_head;
					} else {
						err = ERR_INVALID_ARGS;
					}
					break;
				case _SEEK_CUR:
				case _SEEK_END:
				default:
					err = ERR_INVALID_ARGS;
			}
			mutex_unlock(&v->stream.u.dir.dir_lock);
			break;
		case STREAM_TYPE_PIPE:
			// XXX handle
			break;
		default:
			err = ERR_VFS_WRONG_STREAM_TYPE;
	}

	return err;
}

static int pipefs_ioctl(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, int op, void *buf, size_t len)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *v = _v;
	struct pipefs_cookie *cookie = _cookie;
	int err = 0;

	TRACE(("pipefs_ioctl: vnode 0x%x, cookie 0x%x, op %d, buf 0x%x, len 0x%x\n", _v, _cookie, op, buf, len));

	ASSERT(cookie->s == &v->stream);

	switch(v->stream.type) {
		case STREAM_TYPE_PIPE:
			switch(op) {
				case _PIPEFS_IOCTL_CREATE_ANONYMOUS: {
					int new_fds[2];
					struct pipefs_vnode *new_v;

					if(v != fs->anon_vnode) {
						err = ERR_INVALID_ARGS;
						goto err;
					}

					// create a new vnode
					new_v = pipefs_create_vnode(fs, "", STREAM_TYPE_PIPE);
					if(new_v == NULL) {
						err = ERR_NO_MEMORY;
						goto err;
					}

					// it's an anonymous pipe
					new_v->stream.u.pipe.flags |= PIPE_FLAGS_ANONYMOUS;

					// add it to the hash table
					mutex_lock(&fs->hash_lock);
					hash_insert(fs->vnode_list_hash, new_v);
					mutex_unlock(&fs->hash_lock);

					// open the two endpoints
					new_fds[0] = vfs_open_vnid(fs->id, new_v->id, STREAM_TYPE_PIPE, 0, false);
					if(new_fds[0] < 0) {
						// XXX may leak vnode
						err = new_fds[0];
						goto err;
					}

					new_fds[1] = vfs_open_vnid(fs->id, new_v->id, STREAM_TYPE_PIPE, 0, false);
					if(new_fds[1] < 0) {
						// XXX may leak vnode
						vfs_close(new_fds[0], false);
						err = new_fds[1];
						goto err;
					}

					err = user_memcpy(buf, new_fds, sizeof(new_fds));
					break;
				}
				default:
					err = ERR_INVALID_ARGS;
			}
			break;
		default:
			err = ERR_VFS_WRONG_STREAM_TYPE;
	}

err:
	return err;
}

static int pipefs_canpage(fs_cookie _fs, fs_vnode _v)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t pipefs_readpage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t pipefs_writepage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	return ERR_NOT_ALLOWED;
}

static int pipefs_create(fs_cookie _fs, fs_vnode _dir, const char *name, stream_type st, void *create_args, vnode_id *new_vnid)
{
	// XXX handle named pipes
	return ERR_VFS_READONLY_FS;
}

static int pipefs_unlink(fs_cookie _fs, fs_vnode _dir, const char *name)
{
	struct pipefs *fs = _fs;
	struct pipefs_vnode *dir = _dir;
	struct pipefs_vnode *v;
	int res = NO_ERROR;

	if(dir->stream.type != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	mutex_lock(&dir->stream.u.dir.dir_lock);

	v = pipefs_find_in_dir(dir, name);
	if(!v) {
		res = ERR_NOT_FOUND;
		goto err;
	}

	// XXX make sure it's not the anonymous node
	return ERR_NOT_ALLOWED;

	res = pipefs_remove_from_dir(v->parent, v);
	if( res )
		goto err;

	res = vfs_remove_vnode(fs->id, v->id);

err:
	mutex_unlock(&dir->stream.u.dir.dir_lock);

	return res;
}

static int pipefs_rename(fs_cookie _fs, fs_vnode _olddir, const char *oldname, fs_vnode _newdir, const char *newname)
{
	return ERR_NOT_ALLOWED;
}

static int pipefs_rstat(fs_cookie _fs, fs_vnode _v, struct file_stat *stat)
{
	struct pipefs_vnode *v = _v;

	TRACE(("pipefs_rstat: vnode 0x%x (0x%Lx), stat 0x%x\n", v, v->id, stat));

	stat->vnid = v->id;
	stat->type = v->stream.type;
	stat->size = 0;

	return 0;
}


static int pipefs_wstat(fs_cookie _fs, fs_vnode _v, struct file_stat *stat, int stat_mask)
{
	struct pipefs_vnode *v = _v;

	TOUCH(v);

	TRACE(("pipefs_wstat: vnode 0x%x (0x%Lx), stat 0x%x\n", v, v->id, stat));

	// cannot change anything
	return ERR_NOT_ALLOWED;
}

static struct fs_calls pipefs_calls = {
	&pipefs_mount,
	&pipefs_unmount,
	&pipefs_sync,

	&pipefs_lookup,

	&pipefs_getvnode,
	&pipefs_putvnode,
	&pipefs_removevnode,

	&pipefs_opendir,
	&pipefs_closedir,
	&pipefs_rewinddir,
	&pipefs_readdir,

	&pipefs_open,
	&pipefs_close,
	&pipefs_freecookie,
	&pipefs_fsync,

	&pipefs_read,
	&pipefs_write,
	&pipefs_seek,
	&pipefs_ioctl,

	&pipefs_canpage,
	&pipefs_readpage,
	&pipefs_writepage,

	&pipefs_create,
	&pipefs_unlink,
	&pipefs_rename,

	&pipefs_rstat,
	&pipefs_wstat,
};

int bootstrap_pipefs(void)
{
	dprintf("bootstrap_pipefs: entry\n");

	return vfs_register_filesystem("pipefs", &pipefs_calls);
}

