/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2001-2002, Rob Judd <judd@ob-wan.com>
** Distributed under the terms of the NewOS License.
**
*/

#ifndef _IDE_RAW_H
#define _IDE_RAW_H

#include "ide_private.h"


// ATA register bits

// command block
#define CB_DATA           0   // data reg              in/out pio_base_addr1+0
#define CB_ERR            1   // error reg             in     pio_base_addr1+1
#define CB_FR             1   // feature reg              out pio_base_addr1+1
#define CB_SC             2   // sector count reg      in/out pio_base_addr1+2
#define CB_SN             3   // sector number reg     in/out pio_base_addr1+3
// or block address 0-7
#define CB_CL             4   // cylinder low reg      in/out pio_base_addr1+4
// or block address 8-15
#define CB_CH             5   // cylinder high reg     in/out pio_base_addr1+5
// or block address 16-23
#define CB_DH             6   // drive/head reg        in/out pio_base_addr1+6
#define CB_STAT           7   // primary status reg    in     pio_base_addr1+7
#define CB_CMD            7   // command reg              out pio_base_addr1+7

// control block
#define CB_ASTAT          8   // alternate status reg  in     pio_base_addr2+6
#define CB_DC             8   // device control reg       out pio_base_addr2+6
#define CB_DA             9   // device address reg    in     pio_base_addr2+7

// error register (1)
#define CB_ER_NDAM     0x01   // ATA address mark not found
#define CB_ER_NTK0     0x02   // ATA track 0 not found
#define CB_ER_ABRT     0x04   // ATA command aborted
#define CB_ER_MCR      0x08   // ATA media change request
#define CB_ER_IDNF     0x10   // ATA id not found
#define CB_ER_MC       0x20   // ATA media change
#define CB_ER_UNC      0x40   // ATA uncorrected error
#define CB_ER_BBK      0x80   // ATA bad block
#define CB_ER_ICRC     0x80   // ATA Ultra DMA bad CRC

// drive/head register (6) bits 7-4
#define CB_DH_LBA      0x40   // LBA bit mask
#define CB_DH_DEV0     0xa0   // select device 0
#define CB_DH_DEV1     0xb0   // select device 1

#define DRIVE_SUPPORT_DMA  0x0100 // test mask for DMA support
#define DRIVE_SUPPORT_LBA  0x0200 // test mask for LBA support

// status register (7) bits
#define CB_STAT_ERR    0x01   // error (ATA)
#define CB_STAT_CHK    0x01   // check (ATAPI)
#define CB_STAT_IDX    0x02   // index
#define CB_STAT_CORR   0x04   // corrected
#define CB_STAT_DRQ    0x08   // data request
#define CB_STAT_SKC    0x10   // seek complete
#define CB_STAT_SERV   0x10   // service
#define CB_STAT_DF     0x20   // device fault
#define CB_STAT_WFT    0x20   // write fault (old name)
#define CB_STAT_RDY    0x40   // ready
#define CB_STAT_BSY    0x80   // busy

// device control register (8) bits
#define CB_DC_NIEN     0x02   // disable interrupts
#define CB_DC_SRST     0x04   // soft reset
#define CB_DC_HD15     0x08   // bit should always be set to one


// ATAPI register bits

// error register
#define CB_ER_P_ILI    0x01   // ATAPI illegal length indication
#define CB_ER_P_EOM    0x02   // ATAPI end of media
#define CB_ER_P_ABRT   0x04   // ATAPI command abort
#define CB_ER_P_MCR    0x08   // ATAPI media change request
#define CB_ER_P_SNSKEY 0xf0   // ATAPI sense key mask

// sector count register interrupt reason bits
#define CB_SC_P_CD     0x01   // ATAPI C/D
#define CB_SC_P_IO     0x02   // ATAPI I/O
#define CB_SC_P_REL    0x04   // ATAPI release
#define CB_SC_P_TAG    0xf8   // ATAPI tag (mask)

//**************************************************************

// The ATA/ATAPI command set

// Mandatory commands
#define CMD_EXECUTE_DRIVE_DIAGNOSTIC     0x90
#define CMD_FORMAT_TRACK                 0x50
#define CMD_INITIALIZE_DRIVE_PARAMETERS  0x91
#define CMD_READ_LONG                    0x22 // One sector inc. ECC, with retry
#define CMD_READ_LONG_ONCE               0x23 // One sector inc. ECC, sans retry
#define CMD_READ_SECTORS                 0x20
#define CMD_READ_SECTORS_ONCE            0x21
#define CMD_READ_VERIFY_SECTORS          0x40
#define CMD_READ_VERIFY_SECTORS_ONCE     0x41
#define CMD_RECALIBRATE                  0x10 // Actually 0x10 to 0x1F
#define CMD_SEEK                         0x70 // Actually 0x70 to 0x7F
#define CMD_WRITE_LONG                   0x32
#define CMD_WRITE_LONG_ONCE              0x33
#define CMD_WRITE_SECTORS                0x30
#define CMD_WRITE_SECTORS_ONCE           0x31

// Error codes for CMD_EXECUTE_DRIVE_DIAGNOSTICS
#define DIAG_NO_ERROR                    0x01
#define DIAG_FORMATTER                   0x02
#define DIAG_DATA_BUFFER                 0x03
#define DIAG_ECC_CIRCUITRY               0x04
#define DIAG_MICROPROCESSOR              0x05
#define DIAG_SLAVE_DRIVE_MASK            0x80

// Command codes for CMD_FORMAT_TRACK
#define FMT_GOOD_SECTOR                  0x00
#define FMT_SUSPEND_REALLOC              0x20
#define FMT_REALLOC_SECTOR               0x40
#define FMT_MARK_SECTOR_DEFECTIVE        0x80

// Optional commands
#define CMD_ACK_MEDIA_CHANGE             0xDB
#define CMD_BOOT_POSTBOOT                0xDC
#define CMD_BOOT_PREBOOT                 0xDD
#define CMD_CFA_ERASE_SECTORS            0xC0
#define CMD_CFA_REQUEST_EXT_ERR_CODE     0x03
#define CMD_CFA_TRANSLATE_SECTOR         0x87
#define CMD_CFA_WRITE_MULTIPLE_WO_ERASE  0xCD
#define CMD_CFA_WRITE_SECTORS_WO_ERASE   0x38
#define CMD_CHECK_POWER_MODE             0x98
#define CMD_DEVICE_RESET                 0x08
#define CMD_DOOR_LOCK                    0xDE
#define CMD_DOOR_UNLOCK                  0xDF
#define CMD_FLUSH_CACHE                  0xE7 // CMD_REST
#define CMD_GET_ACOUSTIC_LEVEL           0xAB // proposed for ATA-6
#define CMD_IDENTIFY_DEVICE              0xEC
#define CMD_IDENTIFY_DEVICE_PACKET       0xA1
#define CMD_IDLE                         0x97
#define CMD_IDLE_IMMEDIATE               0x95
#define CMD_NOP                          0x00
#define CMD_PACKET                       0xA0
#define CMD_READ_BUFFER                  0xE4
#define CMD_READ_DMA                     0xC8
#define CMD_READ_DMA_QUEUED              0xC7
#define CMD_READ_MULTIPLE                0xC4
#define CMD_RESTORE_DRIVE_STATE          0xEA
#define CMD_SET_ACOUSTIC_LEVEL           0x2A // proposed for ATA-6
#define CMD_SET_FEATURES                 0xEF
#define CMD_SET_MULTIPLE_MODE            0xC6
#define CMD_SLEEP                        0x99
#define CMD_STANDBY                      0x96
#define CMD_STANDBY_IMMEDIATE            0x94
#define CMD_WRITE_BUFFER                 0xE8
#define CMD_WRITE_DMA                    0xCA
#define CMD_WRITE_DMA_ONCE               0xCB
#define CMD_WRITE_DMA_QUEUED             0xCC
#define CMD_WRITE_MULTIPLE               0xC5
#define CMD_WRITE_SAME                   0xE9
#define CMD_WRITE_VERIFY                 0x3C

// Connor Peripherals' variations
#define CONNOR_CHECK_POWER_MODE          0xE5
#define CONNOR_IDLE                      0xE3
#define CONNOR_IDLE_IMMEDIATE            0xE1
#define CONNOR_SLEEP                     0xE6
#define CONNOR_STANDBY                   0xE2
#define CONNOR_STANDBY_IMMEDIATE         0xE0

//**************************************************************

#if 0
enum {
//  NO_ERROR = 0,
    ERR_TIMEOUT = 1,
    ERR_HARDWARE_ERROR,
    ERR_DRQ_NOT_SET,
    ERR_DISK_BUSY,
    ERR_DEVICE_FAULT,
    ERR_BUFFER_NOT_EMPTY
};
#endif

// generic
uint8 ide_identify_device(int bus, int device);
void  ide_raw_init(unsigned int base1, unsigned int base2);
bool  ide_reset(void);

// ata-specific
int ata_read_sector(ide_device *device, char *buffer, uint32 sector, uint16 numSectors);
int ata_write_sector(ide_device *device, const char *buffer, uint32 sector, uint16 numSectors);
bool  ata_get_partitions(ide_device *device);
uint8 ata_cmd(int bus, int device, int cmd, uint8* buffer);

// atapi-specific


#endif // _ATA_RAW_H
