/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _FAT_H
#define _FAT_H

#include <kernel/vfs.h>
#include "fat_fs.h"

/* mount structure */
typedef struct fat_fs {
	fs_id id;
	int fd;
	void *dev_vnode;
	vnode_id root_vnid;
	sem_id sem;

	int fat_type; // 12/16/32
	uint32 first_data_sector;
	uint32 cluster_count;
	uint16 root_dir_sectors; // always zero on fat 32
	uint32 fat_sector_offset;
	uint32 cluster_size;

	fat_bpb bpb;
	fat_bpb16 bpb16;
	fat_bpb32 bpb32;
} fat_fs;

#define ROOT_VNODE_ID 0 // special vnid that represents the root

/* reader/writer lock for fat */
#define FAT_WRITE_COUNT 1024
#define LOCK_READ(sem) sem_acquire(sem, 1)
#define UNLOCK_READ(sem) sem_release(sem, 1)
#define LOCK_WRITE(sem) sem_acquire(sem, FAT_WRITE_COUNT)
#define UNLOCK_WRITE(sem) sem_release(sem, FAT_WRITE_COUNT)

/* vnode structure */
typedef struct fat_vnode {
	vnode_id id;

	bool is_dir;
	uint32 size;
	uint32 start_cluster; // for fat 12/16 and the root dir, means 'starting sector'
} fat_vnode;

/* fs calls */
int fat_mount(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid);
int fat_unmount(fs_cookie fs);
int fat_sync(fs_cookie fs);

int fat_lookup(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *id);

int fat_getvnode(fs_cookie fs, vnode_id id, fs_vnode *v, bool r);
int fat_putvnode(fs_cookie fs, fs_vnode v, bool r);
int fat_removevnode(fs_cookie fs, fs_vnode v, bool r);

int fat_opendir(fs_cookie fs, fs_vnode v, dir_cookie *cookie);
int fat_closedir(fs_cookie fs, fs_vnode v, dir_cookie cookie);
int fat_rewinddir(fs_cookie fs, fs_vnode v, dir_cookie cookie);
int fat_readdir(fs_cookie fs, fs_vnode v, dir_cookie cookie, void *buf, size_t buflen);

int fat_open(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
int fat_close(fs_cookie fs, fs_vnode v, file_cookie cookie);
int fat_freecookie(fs_cookie fs, fs_vnode v, file_cookie cookie);
int fat_fsync(fs_cookie fs, fs_vnode v);

ssize_t fat_read(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buf, off_t pos, ssize_t len);
ssize_t fat_write(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, ssize_t len);
int fat_seek(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, seek_type st);
int fat_ioctl(fs_cookie fs, fs_vnode v, file_cookie cookie, int op, void *buf, size_t len);

int fat_canpage(fs_cookie fs, fs_vnode v);
ssize_t fat_readpage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);
ssize_t fat_writepage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);

int fat_create(fs_cookie fs, fs_vnode dir, const char *name, void *create_args, vnode_id *new_vnid);
int fat_unlink(fs_cookie fs, fs_vnode dir, const char *name);
int fat_rename(fs_cookie fs, fs_vnode olddir, const char *oldname, fs_vnode newdir, const char *newname);

int fat_mkdir(fs_cookie _fs, fs_vnode _base_dir, const char *name);
int fat_rmdir(fs_cookie _fs, fs_vnode _base_dir, const char *name);

int fat_rstat(fs_cookie fs, fs_vnode v, struct file_stat *stat);
int fat_wstat(fs_cookie fs, fs_vnode v, struct file_stat *stat, int stat_mask);

#endif
