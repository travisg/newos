/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_INCLUDE_SYS_STAT_H
#define _NEWOS_INCLUDE_SYS_STAT_H

#include <sys/types.h>

struct stat {
	dev_t 	st_dev;
	ino_t 	st_ino;
	mode_t	st_mode;
	nlink_t	st_nlink;
	uid_t	st_uid;
	gid_t 	st_gid;
	dev_t	st_rdev;
	time_t	st_atime;
	time_t	st_mtime;
	time_t	st_ctime;
	off_t	st_size;
	blkcnt_t st_blocks;
	blksize_t st_blksize;
};


#endif

