/*
** Copyright 2002-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/debug.h>
#include <kernel/net/misc.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "nfs.h"
#include "nfs_fs.h"
#include "rpc.h"

static inline size_t nfs_pack_string(uint8 *buf, const char *string, size_t maxlen)
{
	size_t stringlen = strlen(string);

	if (stringlen > maxlen)
		stringlen = maxlen;

	*(unsigned int *)buf = htonl(stringlen);
	memcpy(buf + 4, string, stringlen);

	return 4 + ROUNDUP(stringlen, 4);
}

size_t nfs_pack_mountargs(uint8 *buf, const nfs_mountargs *args)
{
	return nfs_pack_string(buf, args->dirpath, MNTPATHLEN);
}

size_t nfs_pack_filename(uint8 *buf, const nfs_filename *name)
{
	return nfs_pack_string(buf, name->name, MAXNAMLEN);
}

size_t nfs_pack_diropargs(uint8 *buf, const nfs_diropargs *args)
{
	memcpy(buf, args->dir, sizeof(nfs_fhandle)); // dir handle
	return sizeof(nfs_fhandle) + nfs_pack_filename(buf + sizeof(nfs_fhandle), &args->name);
}

size_t nfs_pack_readdirargs(uint8 *buf, const nfs_readdirargs *args)
{
	memcpy(buf, args->dir, sizeof(nfs_fhandle)); // dir handle
	buf += sizeof(nfs_fhandle);
	*(unsigned int *)buf = args->cookie;
	*(unsigned int *)(buf + 4) = htonl(args->count);
	return sizeof(nfs_fhandle) + 4 + 4;
}

size_t nfs_pack_readargs(uint8 *buf, const nfs_readargs *args)
{
	memcpy(buf, args->file, sizeof(nfs_fhandle)); // file handle
	buf += sizeof(nfs_fhandle);
	*(unsigned int *)buf = htonl(args->offset);
	*(unsigned int *)(buf + 4) = htonl(args->count);
	*(unsigned int *)(buf + 8) = htonl(args->totalcount);

	return sizeof(nfs_fhandle) + 3 * 4;
}

static inline void nfs_swap_timeval(nfs_timeval *val)
{
	val->seconds = ntohl(val->seconds);
	val->useconds = ntohl(val->useconds);
}

static void nfs_swap_fattr(nfs_fattr *attr)
{
	/* 1:1 mapping, just need to endian swap */
	attr->ftype = ntohl(attr->ftype);
	attr->mode = ntohl(attr->mode);
	attr->nlink = ntohl(attr->nlink);
	attr->uid = ntohl(attr->uid);
	attr->gid = ntohl(attr->gid);
	attr->size = ntohl(attr->size);
	attr->blocksize = ntohl(attr->blocksize);
	attr->rdev = ntohl(attr->rdev);
	attr->blocks = ntohl(attr->blocks);
	attr->fsid = ntohl(attr->fsid);
	attr->fileid = ntohl(attr->fileid);
	nfs_swap_timeval(&attr->atime);
	nfs_swap_timeval(&attr->mtime);
	nfs_swap_timeval(&attr->ctime);
}

void nfs_unpack_diropres(uint8 *buf, nfs_diropres *res)
{
	/* easy one, everything maps 1:1 */
	res->status = ntohl(*(int *)buf);
	buf += 4;

	res->file = (nfs_fhandle *)buf;
	buf += sizeof(nfs_fhandle);

	res->attributes = (nfs_fattr *)buf;
	nfs_swap_fattr(res->attributes);
}

void nfs_unpack_readres(uint8 *buf, nfs_readres *res)
{
	res->status = ntohl(*(int *)buf);
	buf += 4;

	res->attributes = (nfs_fattr *)buf;
	nfs_swap_fattr(res->attributes);
	buf += sizeof(nfs_fattr);

	res->len = ntohl(*(unsigned int *)buf);
	buf += 4;

	res->data = (unsigned char *)buf;
}

void nfs_unpack_attrstat(uint8 *buf, nfs_attrstat *attrstat)
{
	attrstat->status = ntohl(*(int *)buf);
	buf += 4;

	attrstat->attributes = (nfs_fattr *)buf;
	nfs_swap_fattr(attrstat->attributes);
}

