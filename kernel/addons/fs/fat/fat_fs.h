/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _FAT_FS_H
#define _FAT_FS_H

#include <newos/types.h>

#define FAT_BPB_OFFSET 0
#define FAT_BPB_LEN    36

// the FAT BIOS paramenter block (bootsector)
typedef struct fat_bpb {
	uint8  jmpboot[3]; // jmp instruction
	char   oemname[8]; // usually holds "MSWIN4.1"
	uint16 bytes_per_sector;
	uint8  sectors_per_cluster;
	uint16 rsvd_sector_count;
/* 0x10 */
	uint8  num_fats;
	uint16 root_entry_count;
	uint16 total_sectors16;
	uint8  media_type;
	uint16 fat_size_16;
	uint16 sectors_per_track;
	uint16 num_heads;
	uint32 hidden_sectors;
/* 0x20 */
	uint32 total_sectors32;
} fat_bpb;

#define FAT_BPB16_OFFSET 36
#define FAT_BPB16_LEN    26

// depending on the fs type, the next two structures occupy the rest of the on-disk bpb
typedef struct fat_bpb16 {
	uint8  drive_num;
	uint8  reserved;
	uint8  boot_sig;
	uint32 vol_id;
	char   vol_lab[11];
	char   fs_type[8];
} fat_bpb16;

#define FAT_BPB32_OFFSET 36
#define FAT_BPB32_LEN 54

typedef struct fat_bpb32 {
	uint32 fat_size_32;
	uint16 extended_flags;
	uint16 fs_ver;
	uint32 root_cluster;
	uint16 fs_info_cluster;
	uint16 backup_bootsect;
	uint8  drive_num;
	uint8  boot_sig;
	uint32 vol_id;
	char   vol_lab[11];
	char   fs_type[8];
} fat_bpb32;


#endif
