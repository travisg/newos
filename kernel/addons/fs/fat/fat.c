/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/khash.h>
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

#define DEBUG_MSG_PREFIX "FAT -- "

#include <kernel/debug_ext.h>

static uint16 get_16(uint8 *buf) 
{
#if _LITTLE_ENDIAN
	return buf[0] | (buf[1] << 8);
#else
	return buf[1] | (buf[0] << 8);
#endif
}

static uint32 get_32(uint8 *buf) 
{
#if _LITTLE_ENDIAN
	return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
#else
	return buf[3] | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
#endif
}

static int fat_read_bpb(fat_fs *fs)
{
	int err;
	uint8 raw_bpb[FAT_BPB_LEN];
	uint8 raw_bpb16[FAT_BPB16_LEN];
	uint8 raw_bpb32[FAT_BPB32_LEN];
	uint32 data_sectors;
	uint32 cluster_count;

	SHOW_FLOW(3, "fs %p", fs);

	// read in the bios parameter block
	err = sys_read(fs->fd, raw_bpb, FAT_BPB_OFFSET, FAT_BPB_LEN);
	if(err < 0)
		return err;

	// read in the 16-bit part of the second half of the bios parameter block
	err = sys_read(fs->fd, raw_bpb16, FAT_BPB16_OFFSET, FAT_BPB16_LEN);
	if(err < 0)
		return err;

	// read in the 32-bit part of the second half of the bios parameter block
	err = sys_read(fs->fd, raw_bpb32, FAT_BPB32_OFFSET, FAT_BPB32_LEN);
	if(err < 0)
		return err;

	// manually unpack the data into the expanded version of these data structures
	memcpy(&fs->bpb.jmpboot, raw_bpb + 0, 3);
	memcpy(&fs->bpb.oemname, raw_bpb + 3, 8);
	fs->bpb.bytes_per_sector = get_16(raw_bpb + 11);
	fs->bpb.sectors_per_cluster = raw_bpb[13];
	fs->bpb.rsvd_sector_count = get_16(raw_bpb + 14);
	fs->bpb.num_fats = raw_bpb[16];
	fs->bpb.root_entry_count = get_16(raw_bpb + 17);
	fs->bpb.total_sectors16 = get_16(raw_bpb + 19);
	fs->bpb.media_type = raw_bpb[21];
	fs->bpb.fat_size_16 = get_16(raw_bpb + 22);
	fs->bpb.sectors_per_track = get_16(raw_bpb + 24);
	fs->bpb.num_heads = get_16(raw_bpb + 26);
	fs->bpb.hidden_sectors = get_32(raw_bpb + 28);
	fs->bpb.total_sectors32 = get_32(raw_bpb + 32);

	fs->bpb16.drive_num = raw_bpb16[0];
	fs->bpb16.boot_sig = raw_bpb16[2];
	fs->bpb16.vol_id = get_32(raw_bpb16 + 3);
	memcpy(&fs->bpb16.vol_lab, raw_bpb16 + 7, 11);
	memcpy(&fs->bpb16.fs_type, raw_bpb16 + 18, 8);

	fs->bpb32.fat_size_32 = get_32(raw_bpb32);
	fs->bpb32.extended_flags = get_16(raw_bpb32 + 4);
	fs->bpb32.fs_ver = get_16(raw_bpb32 + 6);
	fs->bpb32.root_cluster = get_32(raw_bpb32 + 8);
	fs->bpb32.fs_info_cluster = get_16(raw_bpb32 + 12);
	fs->bpb32.backup_bootsect = get_16(raw_bpb32 + 14);
	fs->bpb32.drive_num = raw_bpb32[28];
	fs->bpb32.boot_sig = raw_bpb32[30];
	fs->bpb32.vol_id = get_32(raw_bpb32 + 31);
	memcpy(&fs->bpb32.vol_lab, raw_bpb32 + 35, 11);
	memcpy(&fs->bpb32.fs_type, raw_bpb32 + 46, 8);

	// calculate some constants
	fs->root_dir_sectors = ((fs->bpb.root_entry_count * 32) + (fs->bpb.bytes_per_sector - 1)) / fs->bpb.bytes_per_sector;
	fs->first_data_sector = fs->bpb.rsvd_sector_count + 
		(fs->bpb.num_fats * ((fs->bpb.fat_size_16 > 0) ? fs->bpb.fat_size_16 : fs->bpb32.fat_size_32)) + fs->root_dir_sectors;
	data_sectors = ((fs->bpb.total_sectors16 > 0) ? fs->bpb.total_sectors16 : fs->bpb.total_sectors32) - fs->first_data_sector;
	cluster_count = data_sectors / fs->bpb.sectors_per_cluster;

	// figure if we're FAT 12/16/32
	if(cluster_count < 4095) {
		fs->fat_type = 12;
	} else if(cluster_count < 65525) {
		fs->fat_type = 16;
	} else {
		fs->fat_type = 32;
	}

	fs->fat_sector_offset = fs->bpb.rsvd_sector_count;
	fs->cluster_size = fs->bpb.sectors_per_cluster * fs->bpb.bytes_per_sector;

	SHOW_INFO(5, "fat_type %d", fs->fat_type);
	SHOW_INFO(5, "root_dir_sectors %d", fs->root_dir_sectors);
	SHOW_INFO(5, "first_data_sector %d", fs->first_data_sector);
	SHOW_INFO(5, "data_sectors %d", data_sectors);
	SHOW_INFO(5, "cluster_count %d", cluster_count);
	SHOW_INFO(5, "fat_sector_offset %d", fs->fat_sector_offset);
	SHOW_INFO(5, "sectors_per_cluster %d (%d bytes)", fs->bpb.sectors_per_cluster, 
		fs->cluster_size);

	// do some sanity checks
	if((fs->cluster_size & (fs->cluster_size - 1)) != 0) 
		return ERR_IO_ERROR; // is it a power of 2
	if(fs->fat_type == 32) {
		if(fs->bpb32.fs_ver != 0) 
			return ERR_IO_ERROR; // unsupported fs version
	}

	return 0;
}

int fat_mount(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid)
{
	fat_fs *fat;
	int err;

	SHOW_FLOW(3, "device %s", device);

	fat = kmalloc(sizeof(fat_fs));
	if(!fat) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	memset(fat, 0, sizeof(fat_fs));

	fat->id = id;

	fat->fd = sys_open(device, 0);
	if(fat->fd < 0) {
		err = fat->fd;
		goto err1;
	}

	err = vfs_get_vnode_from_fd(fat->fd, true, &fat->dev_vnode);
	if(err < 0)
		goto err2;

	// see if we can page from this device
	if(!vfs_canpage(fat->dev_vnode)) {
		err = ERR_VFS_WRONG_STREAM_TYPE;
		goto err3;
	}

	// read in the bios parameter block
	err = fat_read_bpb(fat);
	if(err < 0)
		goto err3;

	// create a semaphore to lock the fs
	fat->sem = sem_create(FAT_WRITE_COUNT, "fat lock");
	if(fat->sem < 0)
		goto err3;

	if(fat->fat_type == 32) {
		fat->root_vnid = CLUSTERS_TO_VNID(fat->bpb32.root_cluster, fat->bpb32.root_cluster);
	} else {
		fat->root_vnid = CLUSTERS_TO_VNID(1, 1);
	}

	*fs = fat;
	*root_vnid = fat->root_vnid;

	return 0;

err3:
	vfs_put_vnode_ptr(fat->dev_vnode);
err2:
	sys_close(fat->fd);
err1:
	kfree(fat);
err:
	return err;
}

int fat_unmount(fs_cookie fs)
{
	fat_fs *fat = (fat_fs *)fs;

	SHOW_FLOW(3, "fat_unmount: fat %p", fat);

	vfs_put_vnode_ptr(fat->dev_vnode);
	sys_close(fat->fd);

	sem_delete(fat->sem);

	kfree(fat);

	return 0;
}

int fat_sync(fs_cookie fs)
{
	SHOW_FLOW(3, "fat_sync: fat %p", fs);

	return 0;
}

static struct fs_calls fat_calls = {
	&fat_mount,
	&fat_unmount,
	&fat_sync,

	&fat_lookup,

	&fat_getvnode,
	&fat_putvnode,
	&fat_removevnode,

	&fat_opendir,
	&fat_closedir,
	&fat_rewinddir,
	&fat_readdir,

	&fat_open,
	&fat_close,
	&fat_freecookie,
	&fat_fsync,

	&fat_read,
	&fat_write,
	&fat_seek,
	&fat_ioctl,

	&fat_canpage,
	&fat_readpage,
	&fat_writepage,

	&fat_create,
	&fat_unlink,
	&fat_rename,

	&fat_mkdir,
	&fat_rmdir,

	&fat_rstat,
	&fat_wstat
};

int fs_bootstrap(void);
int fs_bootstrap(void)
{
	return vfs_register_filesystem("fat", &fat_calls);
}

