/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/net/misc.h>

#include <newos/net.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "nfs.h"
#include "nfs_fs.h"
#include "rpc.h"

#define NFS_TRACE 0

#if NFS_TRACE
#define TRACE(x...) dprintf(x)
#else
#define TRACE(x...)
#endif

static int nfs_getattr(nfs_fs *nfs, nfs_vnode *v, nfs_attrstat *attrstat);

#if NFS_TRACE
static void dump_fhandle(fhandle *handle)
{
	unsigned int i;

	for(i=0; i<sizeof(fhandle); i++)
		dprintf("%02x", ((uint8 *)handle)[i]);
}
#endif

static nfs_vnode *new_vnode_struct(nfs_fs *fs)
{
	nfs_vnode *v;

	v = (nfs_vnode *)kmalloc(sizeof(nfs_vnode));
	if(!v)
		return NULL;

	v->sem = sem_create(1, "nfs vnode sem");
	if(v->sem < 0) {
		kfree(v);
		return NULL;
	}

	v->fs = fs;

	return v;
}

static void destroy_vnode_struct(nfs_vnode *v)
{
	sem_delete(v->sem);
	kfree(v);
}

/* ghetto x.x.x.x:/path parsing code */
static int parse_mount(const char *mount, char *address, int address_len, char *server_path, int path_len)
{
	int a, b;

	// trim the beginning
	for(a = 0; mount[a] != 0 && isspace(mount[a]); a++)
		;
	if(mount[a] == 0)
		return ERR_NOT_FOUND;

	// search for the ':'
	for(b = a; mount[b] != 0 && mount[b] != ':'; b++)
		;
	if(mount[b] == 0)
		return ERR_NOT_FOUND;

	// copy the address out
	memcpy(address, &mount[a], b - a);
	address[b - a] = 0;

	// grab the path
	strcpy(server_path, &mount[b+1]);

	return NO_ERROR;
}

static int parse_ipv4_addr_str(ipv4_addr *ip_addr, char *ip_addr_string)
{
	int a, b;

	*ip_addr = 0;

	// walk through the first number
	a = 0;
	b = 0;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr = atoi(&ip_addr_string[a]) << 24;
	b++;

	// second digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]) << 16;
	b++;

	// third digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]) << 8;
	b++;

	// last digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]);

	return NO_ERROR;
}

static int nfs_mount_fs(nfs_fs *nfs, const char *server_path)
{
	struct mount_args args;
	char buf[128];
	int err;

	memset(&args, sizeof(args), 0);
	strlcpy(args.dirpath, server_path, sizeof(args.dirpath));
	args.len = htonl(strlen(args.dirpath));

	rpc_set_port(&nfs->rpc, MOUNTPORT);

	err = rpc_call(&nfs->rpc, MOUNTPROG, MOUNTVERS, MOUNTPROC_MNT, &args, ntohl(args.len) + 4, buf, sizeof(buf));
	if(err < 0)
		return err;

#if NFS_TRACE
	TRACE("nfs_mount_fs: have root fhandle: ");
	dump_fhandle((fhandle *)&buf[4]);
	TRACE("\n");
#endif
	// we should have the root handle now
	memcpy(&nfs->root_vnode->nfs_handle, &buf[4], sizeof(nfs->root_vnode->nfs_handle));

	return 0;
}

static int nfs_unmount_fs(nfs_fs *nfs)
{
	struct mount_args args;
	int err;

	memset(&args, sizeof(args), 0);
	strlcpy(args.dirpath, nfs->server_path, sizeof(args.dirpath));
	args.len = htonl(strlen(args.dirpath));

	rpc_set_port(&nfs->rpc, MOUNTPORT);

	err = rpc_call(&nfs->rpc, MOUNTPROG, MOUNTVERS, MOUNTPROC_UMNT, &args, ntohl(args.len) + 4, NULL, 0);

	return err;
}

int nfs_mount(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid)
{
	nfs_fs *nfs;
	int err;
	char ip_addr_str[128];
	ipv4_addr ip_addr;

	TRACE("nfs_mount: fsid 0x%x, device '%s'\n", id, device);

	/* create the fs structure */
	nfs = kmalloc(sizeof(nfs_fs));
	if(!nfs) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	memset(nfs, 0, sizeof(nfs_fs));

	mutex_init(&nfs->lock, "nfs lock");

	err = parse_mount(device, ip_addr_str, sizeof(ip_addr_str), nfs->server_path, sizeof(nfs->server_path));
	if(err < 0) {
		err = ERR_NET_BAD_ADDRESS;
		goto err1;
	}

	err = parse_ipv4_addr_str(&ip_addr, ip_addr_str);
	if(err < 0) {
		err = ERR_NET_BAD_ADDRESS;
		goto err1;
	}

	nfs->id = id;
	nfs->server_addr = ip_addr;

	// set up the rpc state
	rpc_init_state(&nfs->rpc);

	// connect
	{
		netaddr server_addr;

		server_addr.type = ADDR_TYPE_IP;
		server_addr.len = 4;
		NETADDR_TO_IPV4(server_addr) = ip_addr;

		rpc_open_socket(&nfs->rpc, &server_addr);
	}

	nfs->root_vnode = new_vnode_struct(nfs);
	nfs->root_vnode->st = STREAM_TYPE_DIR;

	// try to mount the filesystem
	err = nfs_mount_fs(nfs, nfs->server_path);
	if(err < 0)
		goto err2;

	rpc_set_port(&nfs->rpc, NFSPORT);

	*fs = nfs;
	*root_vnid = VNODETOVNID(nfs->root_vnode);

	return 0;

err2:
	rpc_destroy_state(&nfs->rpc);
err1:
	mutex_destroy(&nfs->lock);
	kfree(nfs);
err:
	return err;
}

int nfs_unmount(fs_cookie fs)
{
	nfs_fs *nfs = (nfs_fs *)fs;

	TRACE("nfs_unmount: fsid 0x%x\n", nfs->id);

	// put_vnode on the root to release the ref to it
	vfs_put_vnode(nfs->id, VNODETOVNID(nfs->root_vnode));

	nfs_unmount_fs(nfs);

	rpc_destroy_state(&nfs->rpc);

	mutex_destroy(&nfs->lock);
	kfree(nfs);

	return 0;
}

int nfs_sync(fs_cookie fs)
{
	nfs_fs *nfs = (nfs_fs *)fs;

	TOUCH(nfs);

	TRACE("nfs_sync: fsid 0x%x\n", nfs->id);
	return 0;
}

int nfs_lookup(fs_cookie fs, fs_vnode _dir, const char *name, vnode_id *id)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *dir = (nfs_vnode *)_dir;
	int err;

	TRACE("nfs_lookup: fsid 0x%x, dirvnid 0x%Lx, name '%s'\n", nfs->id, VNODETOVNID(dir), name);

	sem_acquire(dir->sem, 1);

	{
		uint8 buf[sizeof(nfs_diropargs)];
		nfs_diropargs *args = (nfs_diropargs *)buf;
		nfs_diropres  *res  = (nfs_diropres  *)buf;
		int namelen = min(MAXNAMLEN, strlen(name));

		/* set up the args structure */
		memcpy(&args->dir, &dir->nfs_handle, sizeof(args->dir));

		args->name.len = htonl(namelen);
		memcpy(args->name.name, name, namelen);

		err = rpc_call(&nfs->rpc, NFSPROG, NFSVERS, NFSPROC_LOOKUP, args, sizeof(nfs_diropargs), buf, sizeof(buf));
		if(err < 0) {
			err = ERR_NOT_FOUND;
			goto out;
		}

		/* see if the lookup was successful */
		if(htonl(res->status) == NFS_OK) {
			nfs_vnode *new_v;
			nfs_vnode *new_v2;

			/* successful lookup */
#if NFS_TRACE
			dprintf("nfs_lookup: result of lookup of '%s'\n", name);
			dprintf("\tfhandle: "); dump_fhandle(&res->file); dprintf("\n");
			dprintf("\tsize: %d\n", ntohl(res->attributes.size));
#endif
			new_v = new_vnode_struct(nfs);
			if(new_v == NULL) {
				err = ERR_NO_MEMORY;
				goto out;
			}

			/* copy the file handle over */
			memcpy(&new_v->nfs_handle, &res->file, sizeof(new_v->nfs_handle));

			err = vfs_get_vnode(nfs->id, VNODETOVNID(new_v), (fs_vnode *)&new_v2);
			if(err < 0) {
				destroy_vnode_struct(new_v);
				err = ERR_NOT_FOUND;
				goto out;
			}

			ASSERT(new_v == new_v2);

			/* figure out the stream type from the return value and cache it */
			switch(ntohl(res->attributes.ftype)) {
				case NFREG:
					new_v->st = STREAM_TYPE_FILE;
					break;
				case NFDIR:
					new_v->st = STREAM_TYPE_DIR;
					break;
				default:
					new_v->st = -1;
			}

			*id = VNODETOVNID(new_v);
		} else {
			TRACE("nfs_lookup: '%s' not found\n", name);
			err = ERR_NOT_FOUND;
			goto out;
		}
	}

	err = NO_ERROR;

out:
	sem_release(dir->sem, 1);

	return err;
}

int nfs_getvnode(fs_cookie fs, vnode_id id, fs_vnode *v, bool r)
{
	nfs_fs *nfs = (nfs_fs *)fs;

	TOUCH(nfs);

	TRACE("nfs_getvnode: fsid 0x%x, vnid 0x%Lx\n", nfs->id, id);

	*v = VNIDTOVNODE(id);

	return NO_ERROR;
}

int nfs_putvnode(fs_cookie fs, fs_vnode _v, bool r)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);

	TRACE("nfs_putvnode: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	destroy_vnode_struct(v);

	return NO_ERROR;
}

int nfs_removevnode(fs_cookie fs, fs_vnode _v, bool r)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_removevnode: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	return ERR_UNIMPLEMENTED;
}

static int nfs_opendir(fs_cookie _fs, fs_vnode _v, dir_cookie *_cookie)
{
	struct nfs_vnode *v = (struct nfs_vnode *)_v;
	struct nfs_cookie *cookie;
	int err = 0;

	TRACE(("nfs_opendir: vnode 0x%x\n", v));

	if(v->st != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	cookie = kmalloc(sizeof(struct nfs_cookie));
	if(cookie == NULL)
		return ERR_NO_MEMORY;

	cookie->v = v;
	cookie->u.dir.nfscookie = 0;
	cookie->u.dir.at_end = false;

	*_cookie = cookie;

	return err;
}

static int nfs_closedir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie)
{
	struct nfs_fs *fs = _fs;
	struct nfs_vnode *v = _v;
	struct nfs_cookie *cookie = _cookie;

	TOUCH(fs);TOUCH(v);TOUCH(cookie);

	TRACE(("nfs_closedir: entry vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(v->st != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	if(cookie)
		kfree(cookie);

	return 0;
}

static int nfs_rewinddir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie)
{
	struct nfs_vnode *v = _v;
	struct nfs_cookie *cookie = _cookie;
	int err = 0;

	TOUCH(v);

	TRACE(("nfs_rewinddir: vnode 0x%x, cookie 0x%x\n", v, cookie));

	if(v->st != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	sem_acquire(v->sem, 1);

	cookie->u.dir.nfscookie = 0;
	cookie->u.dir.at_end = false;

	sem_acquire(v->sem, 1);

	return err;
}

#define READDIR_BUF_SIZE (MAXNAMLEN + 64)

static ssize_t _nfs_readdir(nfs_fs *nfs, nfs_vnode *v, nfs_cookie *cookie, void *buf, ssize_t len)
{
	uint8 abuf[READDIR_BUF_SIZE];
	nfs_readdirargs *args = (nfs_readdirargs *)abuf;
	nfs_readdirres  *res  = (nfs_readdirres *)abuf;
	ssize_t err = 0;
	int i;
	int namelen;

	if(len < MAXNAMLEN)
		return ERR_VFS_INSUFFICIENT_BUF; // XXX not quite accurate

	/* see if we've already hit the end */
	if(cookie->u.dir.at_end)
		return 0;

	/* put together the message */
	memcpy(&args->dir, &v->nfs_handle, sizeof(args->dir));
	args->cookie = cookie->u.dir.nfscookie;
	args->count = htonl(min(len, READDIR_BUF_SIZE));

	err = rpc_call(&nfs->rpc, NFSPROG, NFSVERS, NFSPROC_READDIR, args, sizeof(*args), abuf, sizeof(abuf));
	if(err < 0)
		return err;

	/* get response */
	if(ntohl(res->status) != NFS_OK)
		return 0;

	/* walk into the buffer, looking for the first entry */
	if(ntohl(res->data[0]) == 0) {
		// end of list
		cookie->u.dir.at_end = true;
		return 0;
	}
	i = ntohl(res->data[0]);

	/* copy the data out of the first entry */
	strlcpy(buf, (char const *)&res->data[i + 2], ntohl(res->data[i + 1]) + 1);

	namelen = ROUNDUP(ntohl(res->data[i + 1]), 4);

	/* update the cookie */
	cookie->u.dir.nfscookie = res->data[i + namelen / 4 + 2];

	return ntohl(res->data[i + 1]);
}

static int nfs_readdir(fs_cookie _fs, fs_vnode _v, dir_cookie _cookie, void *buf, size_t len)
{
	struct nfs_fs *fs = _fs;
	struct nfs_vnode *v = _v;
	struct nfs_cookie *cookie = _cookie;
	int err = 0;

	TOUCH(v);

	TRACE(("nfs_readdir: vnode 0x%x, cookie 0x%x, len 0x%x\n", v, cookie, len));

	if(v->st != STREAM_TYPE_DIR)
		return ERR_VFS_NOT_DIR;

	sem_acquire(v->sem, 1);

	err = _nfs_readdir(fs, v, cookie, buf, len);

	sem_acquire(v->sem, 1);

	return err;
}

int nfs_open(fs_cookie fs, fs_vnode _v, file_cookie *_cookie, int oflags)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_cookie *cookie;
	int err;

	TOUCH(nfs);

	TRACE("nfs_open: fsid 0x%x, vnid 0x%Lx, oflags 0x%x\n", nfs->id, VNODETOVNID(v), oflags);

	if(v->st == STREAM_TYPE_DIR) {
		err = ERR_VFS_IS_DIR;
		goto err;
	}

	cookie = kmalloc(sizeof(nfs_cookie));
	if(cookie == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	cookie->v = v;

	cookie->u.file.pos = 0;
	cookie->u.file.oflags = oflags;

	*_cookie = (file_cookie)cookie;
	err = NO_ERROR;

err:
	return err;
}

int nfs_close(fs_cookie fs, fs_vnode _v, file_cookie _cookie)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_close: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	return NO_ERROR;
}

int nfs_freecookie(fs_cookie fs, fs_vnode _v, file_cookie _cookie)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_cookie *cookie = (nfs_cookie *)_cookie;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_freecookie: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	kfree(cookie);

	return NO_ERROR;
}

int nfs_fsync(fs_cookie fs, fs_vnode _v)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_fsync: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	return NO_ERROR;
}

#define READ_BUF_SIZE 1024

static ssize_t nfs_readfile(nfs_fs *nfs, nfs_vnode *v, nfs_cookie *cookie, void *buf, off_t pos, ssize_t len)
{
	uint8 abuf[4 + sizeof(nfs_fattr) + READ_BUF_SIZE];
	nfs_readargs *args = (nfs_readargs *)abuf;
	nfs_readres  *res  = (nfs_readres *)abuf;
	int err;
	ssize_t total_read = 0;

	/* check args */
	if(pos < 0)
		pos = cookie->u.file.pos;
	/* can't do more than 32-bit offsets right now */
	if(pos > 0xffffffff)
		return 0;
	/* negative or zero length means nothing */
	if(len <= 0)
		return 0;

	while(len > 0) {
		ssize_t to_read = min(len, READ_BUF_SIZE);

		/* put together the message */
		memcpy(&args->file, &v->nfs_handle, sizeof(args->file));
		args->offset = htonl(pos);
		args->count = htonl(to_read);
		args->totalcount = 0; // unused

		err = rpc_call(&nfs->rpc, NFSPROG, NFSVERS, NFSPROC_READ, args, sizeof(*args), abuf, sizeof(abuf));
		if(err < 0)
			break;

		/* get response */
		if(ntohl(res->status) != NFS_OK)
			break;

		/* see how much we read */
		err = user_memcpy((uint8 *)buf + total_read, res->data, ntohl(res->len));
		if(err < 0) {
			total_read = err; // bad user give me bad buffer
			break;
		}

		pos += ntohl(res->len);
		len -= ntohl(res->len);
		total_read += ntohl(res->len);

		/* short read, we're done */
		if((ssize_t)ntohl(res->len) != to_read)
			break;
	}

	cookie->u.file.pos = pos;

	return total_read;
}

ssize_t nfs_read(fs_cookie fs, fs_vnode _v, file_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_cookie *cookie = (nfs_cookie *)_cookie;
	ssize_t err;

	TRACE("nfs_read: fsid 0x%x, vnid 0x%Lx, buf %p, pos 0x%Lx, len %ld\n", nfs->id, VNODETOVNID(v), buf, pos, len);

	if(v->st == STREAM_TYPE_DIR)
		return ERR_VFS_IS_DIR;

	sem_acquire(v->sem, 1);

	err = nfs_readfile(nfs, v, cookie, buf, pos, len);

	sem_release(v->sem, 1);

	return err;
}

#define WRITE_BUF_SIZE 1024

static ssize_t nfs_writefile(nfs_fs *nfs, nfs_vnode *v, nfs_cookie *cookie, const void *buf, off_t pos, ssize_t len)
{
	uint8 abuf[sizeof(nfs_writeargs) + WRITE_BUF_SIZE];
	nfs_writeargs *args = (nfs_writeargs *)abuf;
	nfs_attrstat  *res  = (nfs_attrstat *)abuf;
	int err;
	ssize_t total_written = 0;

	/* check args */
	if(pos < 0)
		pos = cookie->u.file.pos;
	/* can't do more than 32-bit offsets right now */
	if(pos > 0xffffffff)
		return 0;
	/* negative or zero length means nothing */
	if(len <= 0)
		return 0;

	while(len > 0) {
		ssize_t to_write = min(len, WRITE_BUF_SIZE);

		/* put together the message */
		memcpy(&args->file, &v->nfs_handle, sizeof(args->file));
		args->beginoffset = 0; // unused
		args->offset = htonl(pos);
		args->totalcount = 0; // unused
		args->len = htonl(to_write);
		err = user_memcpy(args->data, (const uint8 *)buf + total_written, to_write);
		if(err < 0) {
			total_written = err;
			break;
		}

		err = rpc_call(&nfs->rpc, NFSPROG, NFSVERS, NFSPROC_WRITE, args, sizeof(*args) + to_write, abuf, sizeof(abuf));
		if(err < 0)
			break;

		/* get response */
		if(ntohl(res->status) != NFS_OK)
			break;

		pos += to_write;
		len -= to_write;
		total_written += to_write;
	}

	cookie->u.file.pos = pos;

	return total_written;
}

ssize_t nfs_write(fs_cookie fs, fs_vnode _v, file_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_cookie *cookie = (nfs_cookie *)_cookie;
	ssize_t err;

	TRACE("nfs_write: fsid 0x%x, vnid 0x%Lx, buf %p, pos 0x%Lx, len %ld\n", nfs->id, VNODETOVNID(v), buf, pos, len);

	sem_acquire(v->sem, 1);

	switch(v->st) {
		case STREAM_TYPE_FILE:
			err = nfs_writefile(nfs, v, cookie, buf, pos, len);
			break;
		case STREAM_TYPE_DIR:
			err = ERR_NOT_ALLOWED;
			break;
		default:
			err = ERR_GENERAL;
	}

	sem_release(v->sem, 1);

	return err;
}

int nfs_seek(fs_cookie fs, fs_vnode _v, file_cookie _cookie, off_t pos, seek_type st)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_cookie *cookie = (nfs_cookie *)_cookie;
	int err = NO_ERROR;

	TRACE("nfs_seek: fsid 0x%x, vnid 0x%Lx, pos 0x%Lx, seek_type %d\n", nfs->id, VNODETOVNID(v), pos, st);

	sem_acquire(v->sem, 1);

	switch(v->st) {
		case STREAM_TYPE_FILE: {
			nfs_attrstat attrstat;
			off_t file_len;

			err = nfs_getattr(nfs, v, &attrstat);
			if(err < 0)
				goto out;

			file_len = ntohl(attrstat.attributes.size);

			switch(st) {
				case _SEEK_SET:
					if(pos < 0)
						pos = 0;
					if(pos > file_len)
						pos = file_len;
					cookie->u.file.pos = pos;
					break;
				case _SEEK_CUR:
					if(pos + cookie->u.file.pos > file_len)
						cookie->u.file.pos = file_len;
					else if(pos + cookie->u.file.pos < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos += pos;
					break;
				case _SEEK_END:
					if(pos > 0)
						cookie->u.file.pos = file_len;
					else if(pos + file_len < 0)
						cookie->u.file.pos = 0;
					else
						cookie->u.file.pos = pos + file_len;
					break;
				default:
					err = ERR_INVALID_ARGS;

			}
			break;
		}
		case STREAM_TYPE_DIR:
			switch(st) {
				// only valid args are seek_type _SEEK_SET, pos 0.
				// this rewinds to beginning of directory
				case _SEEK_SET:
					if(pos == 0) {
						cookie->u.dir.nfscookie = 0;
						cookie->u.dir.at_end = false;
					} else {
						err = ERR_INVALID_ARGS;
					}
					break;
				case _SEEK_CUR:
				case _SEEK_END:
				default:
					err = ERR_INVALID_ARGS;
			}
		default:
			err = ERR_INVALID_ARGS;
	}

out:
	sem_release(v->sem, 1);

	return err;
}

int nfs_ioctl(fs_cookie fs, fs_vnode _v, file_cookie cookie, int op, void *buf, size_t len)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_ioctl: fsid 0x%x, vnid 0x%Lx, op %d, buf %p, len %ld\n", nfs->id, VNODETOVNID(v), op, buf, len);

	return ERR_UNIMPLEMENTED;
}

int nfs_canpage(fs_cookie fs, fs_vnode _v)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_canpage: fsid 0x%x, vnid 0x%Lx\n", nfs->id, VNODETOVNID(v));

	return ERR_UNIMPLEMENTED;
}

ssize_t nfs_readpage(fs_cookie fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_readpage: fsid 0x%x, vnid 0x%Lx, vecs %p, pos 0x%Lx\n", nfs->id, VNODETOVNID(v), vecs, pos);

	return ERR_UNIMPLEMENTED;
}

ssize_t nfs_writepage(fs_cookie fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_writepage: fsid 0x%x, vnid 0x%Lx, vecs %p, pos 0x%Lx\n", nfs->id, VNODETOVNID(v), vecs, pos);

	return ERR_UNIMPLEMENTED;
}

int nfs_create(fs_cookie fs, fs_vnode _dir, const char *name, void *create_args, vnode_id *new_vnid)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *dir = (nfs_vnode *)_dir;

	TOUCH(nfs);TOUCH(dir);

	TRACE("nfs_create: fsid 0x%x, vnid 0x%Lx, name '%s'\n", nfs->id, VNODETOVNID(dir), name);

	return ERR_UNIMPLEMENTED;
}

int nfs_unlink(fs_cookie fs, fs_vnode _dir, const char *name)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *dir = (nfs_vnode *)_dir;

	TOUCH(nfs);TOUCH(dir);

	TRACE("nfs_unlink: fsid 0x%x, vnid 0x%Lx, name '%s'\n", nfs->id, VNODETOVNID(dir), name);

	return ERR_UNIMPLEMENTED;
}

int nfs_rename(fs_cookie fs, fs_vnode _olddir, const char *oldname, fs_vnode _newdir, const char *newname)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *olddir = (nfs_vnode *)_olddir;
	nfs_vnode *newdir = (nfs_vnode *)_newdir;

	TOUCH(nfs);TOUCH(olddir);TOUCH(newdir);

	TRACE("nfs_rename: fsid 0x%x, vnid 0x%Lx, oldname '%s', newdir 0x%Lx, newname '%s'\n", nfs->id, VNODETOVNID(olddir), oldname, VNODETOVNID(newdir), newname);

	return ERR_UNIMPLEMENTED;
}

int nfs_mkdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
	nfs_fs *nfs = (nfs_fs *)_fs;
	nfs_vnode *dir = (nfs_vnode *)_base_dir;

	TOUCH(nfs);TOUCH(dir);

	TRACE("nfs_mkdir: fsid 0x%x, vnid 0x%Lx, name '%s'\n", nfs->id, VNODETOVNID(dir), name);

	return ERR_UNIMPLEMENTED;
}

int nfs_rmdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
	nfs_fs *nfs = (nfs_fs *)_fs;
	nfs_vnode *dir = (nfs_vnode *)_base_dir;

	TOUCH(nfs);TOUCH(dir);

	TRACE("nfs_rmdir: fsid 0x%x, vnid 0x%Lx, name '%s'\n", nfs->id, VNODETOVNID(dir), name);

	return ERR_UNIMPLEMENTED;
}

static int nfs_getattr(nfs_fs *nfs, nfs_vnode *v, nfs_attrstat *attrstat)
{
	return rpc_call(&nfs->rpc, NFSPROG, NFSVERS, NFSPROC_GETATTR, &v->nfs_handle, sizeof(v->nfs_handle), attrstat, sizeof(nfs_attrstat));
}

int nfs_rstat(fs_cookie fs, fs_vnode _v, struct file_stat *stat)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;
	nfs_attrstat attrstat;
	int err;

	TRACE("nfs_rstat: fsid 0x%x, vnid 0x%Lx, stat %p\n", nfs->id, VNODETOVNID(v), stat);

	sem_acquire(v->sem, 1);

	err = nfs_getattr(nfs, v, &attrstat);
	if(err < 0)
		goto out;

	if(ntohl(attrstat.status) != NFS_OK) {
		err = ERR_IO_ERROR;
		goto out;
	}

	/* copy the stat over from the nfs attrstat */
	stat->vnid = VNODETOVNID(v);
	stat->size = ntohl(attrstat.attributes.size);
	switch(ntohl(attrstat.attributes.ftype)) {
		case NFREG:
			stat->type = STREAM_TYPE_FILE;
			break;
		case NFDIR:
			stat->type = STREAM_TYPE_DIR;
			break;
		default:
			stat->type = STREAM_TYPE_DEVICE; // XXX should have unknown type
			break;
	}

	err = NO_ERROR;

out:
	sem_release(v->sem, 1);

	return err;
}

int nfs_wstat(fs_cookie fs, fs_vnode _v, struct file_stat *stat, int stat_mask)
{
	nfs_fs *nfs = (nfs_fs *)fs;
	nfs_vnode *v = (nfs_vnode *)_v;

	TOUCH(nfs);TOUCH(v);

	TRACE("nfs_wstat: fsid 0x%x, vnid 0x%Lx, stat %p, stat_mask 0x%x\n", nfs->id, VNODETOVNID(v), stat, stat_mask);

	return ERR_UNIMPLEMENTED;
}

static struct fs_calls nfs_calls = {
	&nfs_mount,
	&nfs_unmount,
	&nfs_sync,

	&nfs_lookup,

	&nfs_getvnode,
	&nfs_putvnode,
	&nfs_removevnode,

	&nfs_opendir,
	&nfs_closedir,
	&nfs_rewinddir,
	&nfs_readdir,

	&nfs_open,
	&nfs_close,
	&nfs_freecookie,
	&nfs_fsync,

	&nfs_read,
	&nfs_write,
	&nfs_seek,
	&nfs_ioctl,

	&nfs_canpage,
	&nfs_readpage,
	&nfs_writepage,

	&nfs_create,
	&nfs_unlink,
	&nfs_rename,

	&nfs_mkdir,
	&nfs_rmdir,

	&nfs_rstat,
	&nfs_wstat
};

int fs_bootstrap(void);
int fs_bootstrap(void)
{
	dprintf("bootstrap_nfs: entry\n");
	return vfs_register_filesystem("nfs", &nfs_calls);
}

