/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2001-2002, Rob Judd <judd@ob-wan.com>
** Distributed under the terms of the NewOS License.
**
** With acknowledgements to Hale Landis <hlandis@ibm.net>
** who wrote the reference implementation.
*/

#include <kernel/arch/cpu.h>
#include <kernel/sem.h>
#include <kernel/debug.h>
#include <nulibc/string.h>

#include "ide_private.h"
#include "ide_raw.h"


// Waste some time by reading the alternate status a few times.
// This gives the drive time to set BUSY in the status register on
// really fast systems. If we don't do this, a slow drive on a fast
// system may not set BUSY fast enough and we would think it had
// completed the command when it really had not even started yet.
#define DELAY400NS  { pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
                      pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); }

// Standard ide base addresses. For pc-card (pcmcia) drives, use
// unused contiguous address block { 100H < (base1=base2) < 3F0H }
// Non-standard addresses also exist. For ATA via sound cards try:
// pio_base2 = 168/368, irq 9 or 10
// pio_base3 = 1e8/3e8, irq 11 or 12
unsigned int pio_base0_addr1 = 0x1f0; // Command block, ide bus 0
unsigned int pio_base0_addr2 = 0x3f0; // Control block, ide bus 0
unsigned int pio_base1_addr1 = 0x170; // Command block, ide bus 1
unsigned int pio_base1_addr2 = 0x370; // Control block, ide bus 1

unsigned int pio_memory_seg = 0;
unsigned int pio_reg_addrs[10];
unsigned char pio_last_read[10];
unsigned char pio_last_write[10];


static uint8 pio_inbyte(uint16 port)
{
  return in8(pio_reg_addrs[port]);
}

static uint16 pio_inword(uint16 port)
{
  return in16(pio_reg_addrs[port]);
}

static void	pio_outbyte(uint16 port, uint8 data)
{
	out8(data, pio_reg_addrs[port]);
}

static void	pio_rep_inword(uint16 port, uint16 *addr, unsigned long count)
{
  __asm__ __volatile__
    (
     "rep ; insw" : "=D" (addr), "=c" (count) : "d" (pio_reg_addrs[port]),
     "0" (addr), "1" (count)
     );
}

static void	pio_rep_outword(uint16 port, uint16 *addr, unsigned long count)
{
  __asm__ __volatile__
    (
     "rep ; outsw" : "=S" (addr), "=c" (count) : "d" (pio_reg_addrs[port]),
     "0" (addr), "1" (count)
     );
}

static void	ide_reg_poll()
{
	while(1)
	{
	  if ((pio_inbyte(CB_ASTAT) & CB_STAT_BSY) == 0)
        break;
	}
}

static bool ide_wait_busy()
{
  int i;

  for(i=0; i<100000; i++)
	{
      if ((pio_inbyte(CB_ASTAT) & CB_STAT_BSY) == 0)
        return true;
	}
  return false;
}

static int ide_select_device(int bus, int device)
{
	uint8		status;
	int			i;
	ide_device	ide = devices[(bus*2) + device];

	// test for a known, valid device
	if(ide.device_type == (NO_DEVICE | UNKNOWN_DEVICE))
      return NO_ERROR;
	// See if we can get it's attention
	if(ide_wait_busy() == false)
	  return ERR_TIMEOUT;
	// Select required device
	pio_outbyte(CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0);
	DELAY400NS;
	for(i=0; i<10000; i++)
      {
		// read the device status
		status = pio_inbyte(CB_STAT);
		if (ide.device_type == ATA_DEVICE) {
		    if ((status & (CB_STAT_BSY | CB_STAT_RDY | CB_STAT_SKC))
                    == (CB_STAT_RDY | CB_STAT_SKC))
			  return NO_ERROR;
		} else {
			if ((status & CB_STAT_BSY) == 0)
			  return NO_ERROR;
		}
      }
  return ERR_TIMEOUT;
}

static void ide_delay(int bus, int device)
{
	ide_device	ide = devices[(bus*2) + device];

	if(ide.device_type == ATAPI_DEVICE)
	  thread_snooze(1000000);

	return;
}

static uint8 reg_pio_data_in(int bus, int dev, int cmd, int fr, int sc,
					unsigned int cyl, int head, int sect, uint8 *output,
					uint16 numSect, unsigned int multiCnt)
{
    unsigned char devHead;
    unsigned char devCtrl;
    unsigned char cylLow;
    unsigned char cylHigh;
    unsigned char status;
    uint16        *buffer = (uint16*)output;
    int           i;

//  dprintf("reg_pio_data_in: bus %d dev %d cmd %d fr %d sc %d cyl %d head %d sect %d numSect %d multiCnt %d\n",
//  	bus, dev, cmd, fr, sc, cyl, head, sect, numSect, multiCnt);

    devCtrl = CB_DC_HD15 | CB_DC_NIEN;
    devHead = dev ? CB_DH_DEV1 : CB_DH_DEV0;
    devHead = devHead | (head & 0x4f);
    cylLow = cyl & 0x00ff;
    cylHigh = (cyl & 0xff00) >> 8;
    // these commands transfer only 1 sector
    if(cmd == (CMD_IDENTIFY_DEVICE | CMD_IDENTIFY_DEVICE_PACKET | CMD_READ_BUFFER))
      numSect = 1;
    // multiCnt = 1 unless CMD_READ_MULTIPLE true
    if(cmd != CMD_READ_MULTIPLE || !multiCnt)
      multiCnt = 1;
    // select the drive
    if(ide_select_device(bus, dev) == ERR_TIMEOUT)
      return ERR_TIMEOUT;
    // set up the registers
    pio_outbyte(CB_DC, devCtrl);
    pio_outbyte(CB_FR, fr);
    pio_outbyte(CB_SC, sc);
    pio_outbyte(CB_SN, sect);
    pio_outbyte(CB_CL, cylLow);
    pio_outbyte(CB_CH, cylHigh);
    pio_outbyte(CB_DH, devHead);
    // Start the command. The drive should immediately set BUSY status.
    pio_outbyte(CB_CMD, cmd);
    DELAY400NS;
    while(1)
    {
      ide_delay(bus, dev);
      // ensure drive isn't still busy
      ide_reg_poll();
      // check status once only per read
      status = pio_inbyte(CB_STAT);
      if((numSect < 1) && (status & CB_STAT_DRQ))
        return ERR_BUFFER_NOT_EMPTY;
      if (numSect < 1)
        break;
      if((status & (CB_STAT_BSY | CB_STAT_DRQ)) == CB_STAT_DRQ)
      {
        unsigned int wordCnt = multiCnt > numSect ? numSect : multiCnt;
        wordCnt = wordCnt * 256;
        pio_rep_inword(CB_DATA, buffer, wordCnt);
        DELAY400NS;
        numSect = numSect - multiCnt;
        buffer += wordCnt;
      }
      // catch all possible fault conditions
      if(status & CB_STAT_BSY)
        return ERR_DISK_BUSY;
      if(status & CB_STAT_DF)
        return ERR_DEVICE_FAULT;
      if(status & CB_STAT_ERR)
        return ERR_HARDWARE_ERROR;
      if((status & CB_STAT_DRQ) == 0)
        return ERR_DRQ_NOT_SET;
    }
    return NO_ERROR;
}

static uint8 reg_pio_data_out(int bus, int dev, int cmd, int fr, int sc,
			     unsigned int cyl, int head, int sect, const uint8 *output,
			     uint16 numSect, unsigned int multiCnt)
{
    unsigned char devHead;
    unsigned char devCtrl;
    unsigned char cylLow;
    unsigned char cylHigh;
    unsigned char status;
    uint16		  *buffer = (uint16*)output;

    devCtrl = CB_DC_HD15 | CB_DC_NIEN;
    devHead = dev ? CB_DH_DEV1 : CB_DH_DEV0;
    devHead = devHead | (head & 0x4f);
    cylLow  = cyl & 0x00ff;
    cylHigh = (cyl & 0xff00) >> 8;
    if (cmd == CMD_WRITE_BUFFER)
      numSect = 1;
    // only Write Multiple and CFA Write Multiple W/O Erase uses multCnt
    if ((cmd != CMD_WRITE_MULTIPLE) && (cmd != CMD_CFA_WRITE_MULTIPLE_WO_ERASE))
      multiCnt = 1;
   // select the drive
    if (ide_select_device(bus, dev) != NO_ERROR)
      return ERR_TIMEOUT;
    // set up the registers
    pio_outbyte(CB_DC, devCtrl);
    pio_outbyte(CB_FR, fr);
    pio_outbyte(CB_SC, sc);
    pio_outbyte(CB_SN, sect);
    pio_outbyte(CB_CL, cylLow);
    pio_outbyte(CB_CH, cylHigh);
    pio_outbyte(CB_DH, devHead);
    // Start the command. The drive should immediately set BUSY status.
    pio_outbyte(CB_CMD, cmd);
    DELAY400NS;
    if (ide_wait_busy() == false)
      return ERR_TIMEOUT;
    status = pio_inbyte(CB_STAT);
    while (1)
    {
      if ((status & (CB_STAT_BSY | CB_STAT_DRQ)) == CB_STAT_DRQ)
      {
        unsigned int wordCnt = multiCnt > numSect ? numSect : multiCnt;
        wordCnt = wordCnt * 256;
        pio_rep_outword(CB_DATA, buffer, wordCnt);
        DELAY400NS;
        numSect = numSect - multiCnt;
        buffer += wordCnt;
      }
      // check all possible fault conditions
      if(status & CB_STAT_BSY)
        return ERR_DISK_BUSY;
      if(status & CB_STAT_DF)
        return ERR_DEVICE_FAULT;
      if(status & CB_STAT_ERR)
        return ERR_HARDWARE_ERROR;
      if ((status & CB_STAT_DRQ) == 0)
        return ERR_DRQ_NOT_SET;
      ide_delay(bus, dev);
      // ensure drive isn't still busy
      ide_reg_poll();
      if(numSect < 1 && status & (CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR))
        {
          dprintf("reg_pio_data_out(): status = 0x%x\n", status);
          return ERR_BUFFER_NOT_EMPTY;
        }
    }
    return NO_ERROR;
}

static void ata_block_to_chs(uint32 block, ide_device *device, int *cylinder, int *head, int *sect)
{
  *sect = (block % device->hardware_device.sectors) + 1;
  block /= device->hardware_device.sectors;
  *head = (block % device->hardware_device.heads);
  block /= device->hardware_device.heads;
  *cylinder = block & 0xFFFF;
//  dprintf("ata_block_to_chs(): block %d -> cyl %d head %d sect %d\n", block, *cylinder, *head, *sect);
}

static void ata_block_to_lba(uint32 block, ide_device *device, int *cylinder, int *head, int *sect)
{
  *sect = block & 0xFF;
  *cylinder = (block >> 8) & 0xFFFF;
  *head = ((block >> 24) & 0xF) | CB_DH_LBA;
//  dprintf("ata_block_to_lba(): block %d -> cyl %d head %d sect %d\n", block, *cylinder, *head, *sect);
}

uint8 ata_read_block(ide_device *device, char *data, uint32 block, uint16 numSectors)
{
  int cyl, head, sect;

  if(device->lba_supported)
    ata_block_to_lba(block, device, &cyl, &head, &sect);
  else
    ata_block_to_chs(block, device, &cyl, &head, &sect);

  return reg_pio_data_in(device->bus, device->device, CMD_READ_SECTORS,
			          0, numSectors, cyl, head, sect, data, numSectors, 2);
}

uint8 ata_write_block(ide_device *device, const char *data, uint32 block, uint16 numSectors)
{
  int cyl, head, sect;

  if(device->lba_supported)
    ata_block_to_lba(block, device, &cyl, &head, &sect);
  else
    ata_block_to_chs(block, device, &cyl, &head, &sect);

  return reg_pio_data_out(device->bus, device->device, CMD_WRITE_SECTORS,
			          0, numSectors, cyl, head, sect, data, numSectors, 2);
}

static void ide_string_conv(char *str, int len)
{
  unsigned int i;
  int	       j;

  for (i=0; i<len/sizeof(unsigned short); i++)
  {
    char c     = str[i*2+1];
    str[i*2+1] = str[i*2];
    str[i*2]   = c;
  }

  str[len - 1] = 0;
  for (j=len-1; j>=0 && str[j]==' '; j--)
    str[j] = 0;
}

// execute a software reset
bool ide_reset(void)
{
  unsigned char devCtrl = CB_DC_HD15 | CB_DC_NIEN;

  // Set and then reset the soft reset bit in the Device
  // Control register.  This causes device 0 be selected
  pio_outbyte(CB_DC, devCtrl | CB_DC_SRST);
  DELAY400NS;
  pio_outbyte(CB_DC, devCtrl);
  DELAY400NS;

  return (ide_wait_busy() ? true : false);
}

// check devices actually exist on bus, and if so what type
static uint8 ide_drive_present(int bus, int device)
{
  int ret = NO_DEVICE;
  unsigned char devCtrl = CB_DC_HD15 | CB_DC_NIEN;
  uint8 sc, sn, ch=0, cl=0, st=0;

  // set up the device control register
  pio_outbyte(CB_DC, devCtrl);
  pio_outbyte(CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0);
  DELAY400NS;
  pio_outbyte(CB_SC, 0x55);
  pio_outbyte(CB_SN, 0xaa);
  pio_outbyte(CB_SC, 0xaa);
  pio_outbyte(CB_SN, 0x55);
  pio_outbyte(CB_SC, 0x55);
  pio_outbyte(CB_SN, 0xaa);
  sc = pio_inbyte(CB_SC);
  sn = pio_inbyte(CB_SN);
  // ensure a device exists
  if(sc == 0x55 && sn == 0xaa)
    ret = UNKNOWN_DEVICE;
  // issue a soft bus reset
  pio_outbyte(CB_DH, CB_DH_DEV0);
  DELAY400NS;
  if(ide_reset() == false)
    dprintf("ide_reset() failed!\n");
  // re-select device
  pio_outbyte(CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0);
  DELAY400NS;
  // test for type of device
  sc = pio_inbyte(CB_SC);
  sn = pio_inbyte(CB_SN);
  if((sc == 0x01) && (sn == 0x01))
    {
      ret = UNKNOWN_DEVICE;
      cl = pio_inbyte(CB_CL);
      ch = pio_inbyte(CB_CH);
      st = pio_inbyte(CB_STAT);
      if ((cl == 0x14) && (ch == 0xeb))
        ret = ATAPI_DEVICE;
      if ((cl == 0x00) && (ch == 0x00) && (st != 0x00))
        ret = ATA_DEVICE;
    }

//  dprintf("ide_drive_present: sector count = %d, sector number = %d,
//    cl = %x, ch = %x, st = %x, return = %d\n", sc, sn, cl, ch, st, ret);

  return (ret ? ret : NO_DEVICE);
}

uint8 ata_cmd(int bus, int device, int cmd, uint8* buffer)
{
return reg_pio_data_in(bus, device, cmd, 1, 0, 0, 0, 0, buffer, 1, 0);
}

uint8 ide_identify_device(int bus, int device)
{
  uint8              *buffer = NULL;
  ide_device *ide  = &devices[(bus*2) + device];

  ide->device_type = ide_drive_present(bus, device);
  ide->bus         = bus;
  ide->device      = device;

//  dprintf("ide_identify_device: type %d, bus %d, device %d\n",
//	  ide->device_type, bus, device);

  switch (ide->device_type) {

    case NO_DEVICE:
    case UNKNOWN_DEVICE:
      break;

    case ATAPI_DEVICE:
      // try for more debug data with optional `identify' command
      buffer = (uint8*)&ide->hardware_device;
      if(ata_cmd(bus, device, CMD_IDENTIFY_DEVICE_PACKET, buffer) == NO_ERROR)
      {
      ide_string_conv(ide->hardware_device.model, 40);
      ide_string_conv(ide->hardware_device.serial, 20);
      ide_string_conv(ide->hardware_device.firmware, 8);

  dprintf("ide: cd-rom at bus %d, device %d, model = %s\n", bus, device,
    ide->hardware_device.model);
  dprintf("ide: cd-rom serial number = %s firmware revision = %s\n",
    ide->hardware_device.serial, ide->hardware_device.firmware);

      }
      break;

    case ATA_DEVICE:
      // try for more debug data with optional `identify' command
      buffer = (uint8*)&ide->hardware_device;
      if(ata_cmd(bus, device, CMD_IDENTIFY_DEVICE, buffer) == NO_ERROR)
      {
      ide->device_type = ATA_DEVICE;
      ide_string_conv(ide->hardware_device.model, 40);
      ide_string_conv(ide->hardware_device.serial, 20);
      ide_string_conv(ide->hardware_device.firmware, 8);
      ide->sector_count = ide->hardware_device.curr_cyls
                        * ide->hardware_device.curr_heads
                        * ide->hardware_device.curr_sectors;
      ide->bytes_per_sector = 512;
      ide->lba_supported = ide->hardware_device.capabilities & DRIVE_SUPPORT_LBA;
      ide->start_block = 0;
      ide->end_block = ide->sector_count; // - ide->start_block;

  dprintf("ide: disk at bus %d, device %d, model = %s\n",
    bus, device, ide->hardware_device.model);
  dprintf("ide: disk serial number = %s firmware revision = %s\n",
    ide->hardware_device.serial, ide->hardware_device.firmware);
  dprintf("ide/%d/%d: %dMB; %d cyl, %d head, %d sec, %d bytes/sec  (LBA=%d)\n",
    bus, device, ide->sector_count * ide->bytes_per_sector / (1000 * 1000),
    ide->hardware_device.curr_cyls, ide->hardware_device.curr_heads,
    ide->hardware_device.curr_sectors, ide->bytes_per_sector, ide->lba_supported);

      }

    default:
      ;
  }
  return (ide->device_type);
}

// set the pio base addresses
void ide_raw_init(unsigned int base1, unsigned int base2)
{
  pio_reg_addrs[CB_DATA] = base1 + 0;  // 0
  pio_reg_addrs[CB_FR  ] = base1 + 1;  // 1
  pio_reg_addrs[CB_SC  ] = base1 + 2;  // 2
  pio_reg_addrs[CB_SN  ] = base1 + 3;  // 3
  pio_reg_addrs[CB_CL  ] = base1 + 4;  // 4
  pio_reg_addrs[CB_CH  ] = base1 + 5;  // 5
  pio_reg_addrs[CB_DH  ] = base1 + 6;  // 6
  pio_reg_addrs[CB_CMD ] = base1 + 7;  // 7

  pio_reg_addrs[CB_DC  ] = base2 + 6;  // 8
  pio_reg_addrs[CB_DA  ] = base2 + 7;  // 9
}

static bool ata_get_partition_info(ide_device *device, tPartition *partition, uint32 position)
{
	char buffer[512];
	uint8* partitionBuffer = buffer;

	if (ata_read_block(device, buffer, position, 1) != NO_ERROR) {

	dprintf("ata_get_partition_info(): unable to read partition table\n");

		return false;
	}

	if ((partitionBuffer[PART_IDENT_OFFSET]   != 0x55) ||
	    (partitionBuffer[PART_IDENT_OFFSET+1] != 0xaa)) {

	dprintf("ata_get_partition_info(): partition table signature is incorrect\n");

		return false;
	}

	memcpy(partition, partitionBuffer + PARTITION_OFFSET, sizeof(tPartition) * NUM_PARTITIONS);

	return true;
}

bool ata_get_partitions(ide_device *device)
{
  uint8 i, j;

  memset(&device->partitions, 0, sizeof(tPartition) * MAX_PARTITIONS);
  if(ata_get_partition_info(device, device->partitions, 0) == false)
    return false;

  dprintf("Primary Partition Table\n");
  for (i = 0; i < NUM_PARTITIONS; i++)
    {
      dprintf("  %d: flags:%x type:%x start:cy%d hd%d sc%d end:cy%d hd%d sc%d lba:%d len:%d\n",
	  i,
	  device->partitions[i].boot_flags,
	  device->partitions[i].partition_type,
	  device->partitions[i].starting_cylinder,
	  device->partitions[i].starting_head,
      device->partitions[i].starting_sector,
	  device->partitions[i].ending_cylinder,
	  device->partitions[i].ending_head,
	  device->partitions[i].ending_sector,
	  device->partitions[i].starting_block,
	  device->partitions[i].sector_count);
    }

    for(j=0; j<4; j++)
    {
    if((device->partitions[j].partition_type == PTDosExtended)      ||
       (device->partitions[j].partition_type == PTWin95ExtendedLBA) ||
       (device->partitions[j].partition_type == PTLinuxExtended))
    {
      int extOffset = device->partitions[j].starting_block;
      if(ata_get_partition_info(device, &device->partitions[4], extOffset) == false)
        return false;
      dprintf("Extended Partition Table\n");
      for (i=NUM_PARTITIONS; i<MAX_PARTITIONS; i++)
        {
          device->partitions[i].starting_block += extOffset;
          dprintf("  %d: flags:%x type:%x start:cy%d hd%d sc%d end:cy%d hd%d sc%d lba:%d len:%d\n",
		  i,
		  device->partitions[i].boot_flags,
		  device->partitions[i].partition_type,
		  device->partitions[i].starting_cylinder,
		  device->partitions[i].starting_head,
		  device->partitions[i].starting_sector,
		  device->partitions[i].ending_cylinder,
		  device->partitions[i].ending_head,
		  device->partitions[i].ending_sector,
		  device->partitions[i].starting_block,
		  device->partitions[i].sector_count);
        }
    }
    }
  return true;
}
