/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002,  Rob Judd <judd@ob-wan.com>
** Distributed under the terms of the NewOS License.
**
*/

#ifndef _PARTITION_H
#define _PARTITION_H

#define  PART_IDENT_OFFSET  0x01fe
#define  PARTITION_OFFSET   0x01be
#define  NUM_PARTITIONS     4

#define  PTCHSToLBA(c, h, s, scnt, hcnt) ((s) & 0x3f) + \
    (scnt) * ( (h) + (hcnt) * ((c) | (((s) & 0xc0) << 2)))
#define  PTLBAToCHS(lba, c, h, s, scnt, hcnt) ( \
    (s) = (lba) % (scnt) + 1,  \
    (lba) /= (scnt), \
    (h) = (lba) % (hcnt), \
    (lba) /= (heads), \
    (c) = (lba) & 0xff, \
    (s) |= ((lba) >> 2) & 0xc0)

// reference for all known partition types can be found here:
// http://www.win.tue.nl/~aeb/partitions/partition_types.html
typedef enum PartitionTypes {
    PTEmpty = 0,
    PTDOS3xPrimary,     /*  1 */
    PTXENIXRoot,        /*  2 */
    PTXENIXUsr,         /*  3 */
    PTOLDDOS16Bit,      /*  4 */
    PTDosExtended,      /*  5 */
    PTDos5xPrimary,     /*  6 */
    PTOS2HPFS,          /*  7 */
    PTAIX,              /*  8 */
    PTAIXBootable,      /*  9 */
    PTOS2BootMgr,       /* 10 */
    PTWin95FAT32,
    PTWin95FAT32LBA,
    PTWin95FAT16LBA =    0x0e,
    PTWin95ExtendedLBA = 0x0f,
    PTVenix286 =         0x40,
    PTNovell =           0x51,
    PTMicroport =        0x52,
    PTGnuHurd =          0x63,
    PTNetware286 =       0x64,
    PTNetware386 =       0x65,
    PTPCIX =             0x75,
    PTOldMinix =         0x80,
    PTMinix =            0x81,
    PTLinuxSwap =        0x82,
    PTLinuxExt2 =        0x83,
    PTLinuxExtended =    0x85,
    PTAmoeba =           0x93,
    PTAmoebaBBT =        0x94,
    PTBSD =              0xa5,
    PTBSDIFS =           0xb7,
    PTBSDISwap =         0xb8,
    PTSyrinx =           0xc7,
    PTCPM =              0xdb,
    PTDOSAccess =        0xe1,
    PTDOSRO =            0xe3,
    PTBeOS =             0xeb,
    PTDOSSecondary =     0xf2,
    PTBBT =              0xff
} partitionTypes;

typedef struct sPartition {
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

} tPartition;

#endif
