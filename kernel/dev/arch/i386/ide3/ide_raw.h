/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
**
** Modified Sep 2001 by Rob Judd <judd@ob-wan.com>
*/

#ifndef __IDE_RAW_H__
#define __IDE_RAW_H__

#include "ide_private.h"

enum {
//  NO_ERROR = 0,
  ERR_TIMEOUT = 1,
  ERR_HARDWARE_ERROR,
  ERR_DRQ_NOT_SET,
  ERR_DISK_BUSY,
  ERR_DEVICE_FAULT,
  ERR_BUFFER_NOT_EMPTY
};

uint8 ide_read_block(ide_device *device, char *buffer, uint32 block, uint8 numSectors);
uint8 ide_write_block(ide_device *device, const char *buffer, uint32 block, uint8 numSectors);
uint8 ide_identify_device(int bus, int device);
void  ide_raw_init(int base1, int base2);
bool  ide_get_partitions(ide_device *device);
int   ide_reset(void);

#endif
