#ifndef __IDE_DRIVE_H__
#define	__IDE_DRIVE_H__
#include <newos/types.h>
#define	DRIVE_PRESENT			0
#define	DRIVE_PRESENT_AND_MANAGED	1
#define	NO_DRIVE	-1
#define	MAX_PARTITION_PER_DRIVE		4
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
} ide_drive_info;

#define  PARTITION_MAGIC1	0x55
#define  PARTITION_MAGIC2	0xaa
#define  PART_MAGIC_OFFSET	0x01fe
#define  PARTITION_OFFSET	0x01be


#define  PTCHSToLBA(c, h, s, scnt, hcnt) ((s) & 0x3f) + \
    (scnt) * ( (h) + (hcnt) * ((c) | (((s) & 0xc0) << 2)))
#define  PTLBAToCHS(lba, c, h, s, scnt, hcnt) ( \
    (s) = (lba) % (scnt) + 1,  \
    (lba) /= (scnt), \
    (h) = (lba) % (hcnt), \
    (lba) /= (heads), \
    (c) = (lba) & 0xff, \
    (s) |= ((lba) >> 2) & 0xc0)

/* taken from linux fdisk src */
typedef enum PartitionTypes
{
  PTEmpty = 0,
  PTDOS3xPrimary,  /*  1 */
  PTXENIXRoot,     /*  2 */
  PTXENIXUsr,      /*  3 */
  PTOLDDOS16Bit,   /*  4 */
  PTDosExtended,   /*  5 */
  PTDos5xPrimary,  /*  6 */
  PTOS2HPFS,       /*  7 */
  PTAIX,           /*  8 */
  PTAIXBootable,   /*  9 */
  PTOS2BootMgr,    /* 10 */
  PTWin95FAT32,
  PTWin95FAT32LBA,
  PTWin95FAT16LBA,
  PTWin95ExtendedLBA,
  PTVenix286 =      0x40,
  PTNovell =        0x51,
  PTMicroport =     0x52,
  PTGnuHurd =       0x63,
  PTNetware286 =    0x64,
  PTNetware386 =    0x65,
  PTPCIX =          0x75,
  PTOldMinix =      0x80,
  PTMinix =         0x81,
  PTLinuxSwap =     0x82,
  PTLinuxExt2 =     0x83,
  PTAmoeba =        0x93,
  PTAmoebaBBT =     0x94,
  PTBSD =           0xa5,
  PTBSDIFS =        0xb7,
  PTBSDISwap =      0xb8,
  PTSyrinx =        0xc7,
  PTCPM =           0xdb,
  PTDOSAccess =     0xe1,
  PTDOSRO =         0xe3,
  PTDOSSecondary =  0xf2,
  PTBBT =           0xff
} partitionTypes;

#define PartitionIsExtended(P) ((P)->PartitionType == PTDosExtended)

typedef struct sPartition
{
  unsigned char   boot_flags;
  unsigned char   starting_head;
  unsigned char   starting_sector;
  unsigned char   starting_cylinder;
  unsigned char   partition_type;
  unsigned char   ending_head;
  unsigned char   ending_sector;
  unsigned char   ending_cylinder;
  unsigned int    starting_block;
  unsigned int    sector_count;

} partition_info;
typedef	struct
{
	char		*device_name;
	void		*(*init_drive)(ide_bus *bus,void *b_cookie,int channel,int drive);
	int		(*read_block)(void *b_cookie,void *d_cookie,long block,void *buffer,size_t size);
	int		(*write_block)(void *b_cookie,void *d_cookie,long block,const void *buffer,size_t size);
	uint16		(*get_bytes_per_sector)(void *d_cookie);
	int		(*ioctl)(void *b_cookie,void *d_cookie,int command,void *buffer,size_t size);
	void		(*signal_interrupt)(void *b_cookie,void *d_cookie);
	partition_info	partitions[2*MAX_PARTITION_PER_DRIVE];
}ide_drive;
extern	ide_drive	*ide_drives[];
#endif
