/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
**
** Modified Sep 2001 by Rob Judd <judd@ob-wan.com>
*/

#ifndef __IDE_RAW_H__
#define __IDE_RAW_H__

#include "ide_private.h"

#define NO_ERROR             0
#define ERR_TIMEOUT          1
#define ERR_HARDWARE_ERROR   2
#define ERR_DRQ_NOT_SET      3
#define ERR_DISK_BUSY        4
#define ERR_DEVICE_FAULT     5
#define ERR_BUFFER_NOT_EMPTY 6

int  ide_read_block(ide_device *device, char *buffer, uint32 block, uint8 numSectors);
int  ide_write_block(ide_device *device, const char *buffer, uint32 block, uint8 numSectors);
int  ide_base(int bus);
void ide_string_conv (char *str, int len);
int  ide_reset(void);
int  ide_drive_present(int bus, int device);
int  ide_identify_device(int bus, int device);
void ide_raw_init(int base1, int base2);
bool ide_get_partitions(ide_device *device);
int  ide_get_acoustic(ide_device *device, int8* level_ptr);
int  ide_set_acoustic(ide_device *device, int8 level);

#endif
