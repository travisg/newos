/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2001-2002, Rob Judd <judd@ob-wan.com>
** Distributed under the terms of the NewOS License.
**
*/

#ifndef _IDE_PRIVATE_H
#define	_IDE_PRIVATE_H

#include <newos/types.h>

#include "ata_partition.h"

typedef struct
{
	unsigned short 	config;               /* obsolete stuff */
	unsigned short 	cyls;                 /* default logical cylinders */
	unsigned short 	_reserved_2;
	unsigned short 	heads;                /* default logical heads */
	unsigned short 	_vendor_4;            /* bytes per track unformatted */
	unsigned short 	_vendor_5;            /* sectors per track unformatted */
	unsigned short 	sectors;              /* default logical sectors */
	unsigned short 	_vendor_7;
	unsigned short 	_vendor_8;
	unsigned short 	_vendor_9;
	char            serial[20]; //40      /* serial number */
	unsigned short 	_vendor_20;
	unsigned short 	_vendor_21;
	unsigned short 	vend_bytes_long;      /* number of ECC bytes on long cmd */
	char            firmware[8];          /* firmware revision */
	char            model[40];  //94      /* model name */
	unsigned short 	mult_support;         /* vendor stuff and multiple cmds */
	unsigned short 	_reserved_48;         /* bit 0: double word i/o possible */
	unsigned short 	capabilities;         /* bit 9: LBA;  bit 8: DMA support */
	unsigned short 	_reserved_50;
	unsigned short 	pio;                  /* bit 15-8: timing mode for PIO */
	unsigned short 	dma;                  /* bit 15-8: timing mode for DMA */
	unsigned short 	_reserved_53;
	unsigned short 	curr_cyls;            /* current logical cylinders */
	unsigned short 	curr_heads;           /* current logical heads */
	unsigned short 	curr_sectors;         /* current logical sectors */
	unsigned long 	capacity;   //118     /* apparent capacity in sectors */
	unsigned short 	_pad[511-59];         /* don't need this stuff for now */
} ide_hw_id_t;

#define IDE_MAGIC_COOKIE1       0xABCDEF10
#define IDE_MAGIC_COOKIE2       0x12345678

enum
{
  NO_DEVICE = 0,
  UNKNOWN_DEVICE,
  ATA_DEVICE,
  ATAPI_DEVICE
};

typedef	struct s_ide_device
{
  uint32        magic1;
  ide_hw_id_t   hardware_device;
  uint32        magic2;
  uint16        iobase;
  int           bus;
  int           device;
  uint8         device_type;
  uint8         _reserved;      // used for pointer return to ide.c
  uint32        sector_count;
  uint32        bytes_per_sector;
  bool          lba_supported;
  uint32        start_block;
  uint32        end_block;
  tPartition    partitions[8];  // 4 Primary + 4 Extended
} ide_device;

#define	MAX_IDE_BUSES   2
#define	MAX_IDE_DEVICES (2 * MAX_IDE_BUSES)
#define MAX_PARTITIONS  8

extern	ide_device      devices[MAX_IDE_DEVICES];

enum {
  DISK_EXECUTE_DIAGNOSTIC = 1,
  DISK_GET_GEOMETRY,
  DISK_USE_DMA,
  DISK_USE_BUS_MASTERING,
  DISK_USE_PIO,
};

typedef struct s_drive_geometry
{
  uint32	blocks;
  uint32	heads;
  uint32	cylinders;
  uint32	sectors;
  bool		removable;
  uint32	bytes_per_sector;
  bool		read_only;
  char		model[40];
  char		serial[20];
  char		firmware[8];
} drive_geometry;

#endif // _IDE_PRIVATE_H
