/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/debug.h>

#include <string.h>

#include "fat.h"

#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#define DEBUG_MSG_PREFIX "FAT_FILE -- "

#include <kernel/debug_ext.h>

int fat_open(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

int fat_close(fs_cookie fs, fs_vnode v, file_cookie cookie)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

int fat_freecookie(fs_cookie fs, fs_vnode v, file_cookie cookie)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

int fat_fsync(fs_cookie fs, fs_vnode v)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

ssize_t fat_read(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	SHOW_FLOW(3, "fs %p, v %p, buf %p, pos %Ld, len %ld", fs, v, buf, pos, len);

	return ERR_NOT_IMPLEMENTED;
}

ssize_t fat_write(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	SHOW_FLOW(3, "fs %p, v %p, buf %p, pos %Ld, len %ld", fs, v, buf, pos, len);

	return ERR_NOT_IMPLEMENTED;
}

int fat_seek(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, seek_type st)
{
	SHOW_FLOW(3, "fs %p, v %p, pos %Ld, st %d", fs, v, pos, st);

	return ERR_NOT_IMPLEMENTED;
}

int fat_ioctl(fs_cookie fs, fs_vnode v, file_cookie cookie, int op, void *buf, size_t len)
{
	SHOW_FLOW(3, "fs %p, v %p, op %d, buf %p, len %ld", fs, v, op, buf, len);

	return ERR_NOT_IMPLEMENTED;
}

