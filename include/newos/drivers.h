/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <newos/types.h>
#include <newos/defines.h>

enum {
	// called on partition device to get info
	IOCTL_DEVFS_GET_PARTITION_INFO = 1000,
	// called on raw device to declare one partition
	IOCTL_DEVFS_SET_PARTITION,

	// framebuffer ops
	IOCTL_DEVFS_GET_FRAMEBUFFER_INFO = 2000,
	IOCTL_DEVFS_MAP_FRAMEBUFFER,
};

typedef struct devfs_partition_info {
	// offset and size relative to raw device in bytes
	off_t offset;
	off_t size;

	// logical block size in bytes
	uint32 logical_block_size;

	// session/partition id
	uint32 session;
	uint32 partition;

	// path of raw device (GET_PARTITION_INFO only)
	char raw_device[SYS_MAX_PATH_LEN];
} devfs_partition_info;

// framebuffer stuff
enum {
	COLOR_SPACE_UNKNOWN = 0,
	COLOR_SPACE_8BIT,
	COLOR_SPACE_RGB555,
	COLOR_SPACE_RGB565,
	COLOR_SPACE_RGB888,
};

typedef struct devfs_framebuffer_info {
	int width;
	int height;
	int bit_depth;
	int color_space;
} devfs_framebuffer_info;

