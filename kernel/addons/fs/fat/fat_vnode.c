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
#include <kernel/sem.h>

#include <string.h>

#include "fat.h"

#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#define DEBUG_MSG_PREFIX "FAT_VNODE -- "

#include <kernel/debug_ext.h>


int fat_getvnode(fs_cookie fs, vnode_id id, fs_vnode *_v, bool r)
{
	fat_fs *fat = (fat_fs *)fs;
	fat_vnode *v;
	int err;
	uint32 dir_cluster, file_cluster;

	SHOW_FLOW(3, "fs %p, vnode_id 0x%Lx, r %d", fs, id, r);

	LOCK_READ(fat->sem);

	// break the vnid out into the parts
	dir_cluster = VNID_TO_DIR_CLUSTER(id);
	file_cluster = VNID_TO_FILE_CLUSTER(id);

	v = kmalloc(sizeof(fat_vnode));
	if(!v) {
		err = ERR_NO_MEMORY;
		goto out;
	}

	v->id = id;
	v->start_cluster = file_cluster;

	if(v->id == fat->root_vnid) {
		// special case the root vnode for size
		if(fat->fat_type == 32) {
			v->size = 0; // XXX count clusters
		} else {
			v->size = fat->bpb.root_entry_count * 32;
		}
	}

	*_v = v;

	err = NO_ERROR;

out:
	UNLOCK_READ(fat->sem);

	return err;
}

int fat_putvnode(fs_cookie fs, fs_vnode _v, bool r)
{
	fat_fs *fat = (fat_fs *)fs;
	fat_vnode *v = (fat_vnode *)_v;

	SHOW_FLOW(3, "fs %p, v %p, r %d", fs, v, r);

	LOCK_READ(fat->sem);

	kfree(v);
	
	UNLOCK_READ(fat->sem);

	return NO_ERROR;
}

int fat_removevnode(fs_cookie fs, fs_vnode v, bool r)
{
	SHOW_FLOW(3, "fs %p, v %p, r %d", fs, v, r);

	return ERR_NOT_IMPLEMENTED;
}

int fat_canpage(fs_cookie fs, fs_vnode v)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

ssize_t fat_readpage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

ssize_t fat_writepage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

int fat_create(fs_cookie fs, fs_vnode dir, const char *name, void *create_args, vnode_id *new_vnid)
{
	SHOW_FLOW(3, "fs %p, dir %p, name '%s'", fs, dir, name);

	return ERR_NOT_IMPLEMENTED;
}

int fat_unlink(fs_cookie fs, fs_vnode dir, const char *name)
{
	SHOW_FLOW(3, "fs %p, dir %p, name '%s'", fs, dir, name);

	return ERR_NOT_IMPLEMENTED;
}

int fat_rename(fs_cookie fs, fs_vnode olddir, const char *oldname, fs_vnode newdir, const char *newname)
{
	SHOW_FLOW(3, "fs %p, olddir %p, oldname '%s', newdir %p, newname '%s'", fs, olddir, oldname, newdir, newname);

	return ERR_NOT_IMPLEMENTED;
}

int fat_rstat(fs_cookie fs, fs_vnode _v, struct file_stat *stat)
{
	fat_fs *fat = (fat_fs *)fs;
	fat_vnode *v = (fat_vnode *)_v;

	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	LOCK_READ(fat->sem);

	stat->vnid = v->id;
	stat->type = v->is_dir ? STREAM_TYPE_DIR : STREAM_TYPE_FILE;
	stat->size = v->size;

	UNLOCK_READ(fat->sem);

	return 0;
}

int fat_wstat(fs_cookie fs, fs_vnode v, struct file_stat *stat, int stat_mask)
{
	SHOW_FLOW(3, "fs %p, v %p", fs, v);

	return ERR_NOT_IMPLEMENTED;
}

