#ifndef __IDE_PRIVATE_H__
#define	__IDE_PRIVATE_H__
#include <types.h>
#include "partition.h"
typedef struct
{
	unsigned short 	config;               /* obsolete stuff */
	unsigned short 	cyls;                 /* logical cylinders */
	unsigned short 	_reserved_2;
	unsigned short 	heads;                /* logical heads */
	unsigned short 	_vendor_4;
	unsigned short 	_vendor_5;
	unsigned short 	sectors;              /* logical sectors */
	unsigned short 	_vendor_7;
	unsigned short 	_vendor_8;
	unsigned short 	_vendor_9;
	char 		serial[20]; //40                    /* serial number */
	unsigned short 	_vendor_20;
	unsigned short 	_vendor_21;
  unsigned short 	vend_bytes_long;      /* no. vendor bytes on long cmd */
	char 		firmware[8];
	char 		model[40]; //94
	unsigned short 	mult_support;         /* vendor stuff and multiple cmds */
	unsigned short 	_reserved_48;
	unsigned short 	capabilities;
	unsigned short 	_reserved_50;
	unsigned short 	pio;
	unsigned short 	dma;
	unsigned short 	_reserved_53;
	unsigned short 	curr_cyls;            /* current logical cylinders */
	unsigned short 	curr_heads;           /* current logical heads */
	unsigned short 	curr_sectors;         /* current logical sectors */
	unsigned long 	capacity;     //118          /* capacity in sectors */
	unsigned short 	_pad[512-59];         /* don't need this stuff for now */
} ide_hw_id_t;

#define IDE_PRI_BASE                0x1f0
#define IDE_SEC_BASE                0x170

#define	IDE_MAGIC_COOKIE	    0xABCDEF10
#define	IDE_MAGIC_COOKIE2	    0x12345678
#define	NO_DEVICE			-1
#define	UNKNOWN_DEVICE			0
#define	ATA_DEVICE			1
#define	ATAPI_DEVICE			2
#define	IDE_NAME			40

typedef	struct s_ide_device
{
  uint32		magic;
  ide_hw_id_t		hardware_device;
  uint32		magic2;
  int			iobase;
  int			bus;
  int			device;
  int			device_type;
  uint32		sector_count;
  int			bytes_per_sector;
  bool			lba_supported;
  int			start_block;
  int			end_block;
  tPartition		partitions[8]; // 4 Primary + 4 Extended
}ide_device;
#define	MAX_DEVICES	4
#define	MAX_IDE_BUSES	2

extern	ide_device		devices[MAX_DEVICES];
#define	DISK_GET_GEOMETRY	1
#define	DISK_USE_DMA		2
#define	DISK_USE_BUS_MASTERING	3
#define	DISK_USE_PIO		4
typedef	struct	s_drive_geometry
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
}drive_geometry;

#endif

