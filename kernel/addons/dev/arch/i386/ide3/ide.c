/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2001-2002, Rob Judd <judd@ob-wan.com>
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <newos/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <nulibc/string.h>
#include <nulibc/stdio.h>

#include <kernel/dev/arch/i386/ide/ide_bus.h>

#include <kernel/fs/devfs.h>

#include "ide_private.h"
#include "ide_raw.h"

ide_device      devices[MAX_IDE_DEVICES];
sem_id ide_sem, ide_sem_0, ide_sem_1;

#define IDE_0_INTERRUPT  14
#define IDE_1_INTERRUPT  15

typedef struct
{
	ide_device* dev;

	// specs for whole disk or partition
	uint32		block_start;
	uint32		block_count;
	uint16		block_size;
} ide_ident;

//--------------------------------------------------------------------------------
static int ide_open(dev_ident _ident, dev_cookie *cookie)
{
	ide_ident* ident = (ide_ident*)_ident;

	*cookie = ident;

	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ide_close(dev_cookie _cookie)
{
	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ide_freecookie(dev_cookie cookie)
{
	// nothing to free here, as cookie is also dev_ident
	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ata_execute_diagnostic(ide_device* dev)
{
	uint8 err;
	uint8* buf = (uint8*)&dev->_reserved;

	err = ata_cmd(dev->bus, 0, CMD_EXECUTE_DRIVE_DIAGNOSTIC, buf);
	if(err == NO_ERROR)
		return (int)buf;

	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ata_get_geometry(ide_device* device, void *buf, size_t len)
{
	drive_geometry* drive_geometry = buf;

	if (len < sizeof(drive_geometry))
		return ERR_VFS_INSUFFICIENT_BUF;

	drive_geometry->blocks = device->end_block - device->start_block;
	drive_geometry->heads = device->hardware_device.curr_heads;
	drive_geometry->cylinders = device->hardware_device.curr_cyls;
	drive_geometry->sectors = device->hardware_device.curr_sectors;
	drive_geometry->removable = false;
	drive_geometry->bytes_per_sector = device->bytes_per_sector;
	drive_geometry->read_only = false;
	strcpy(drive_geometry->model, device->hardware_device.model);
	strcpy(drive_geometry->serial, device->hardware_device.serial);
	strcpy(drive_geometry->firmware, device->hardware_device.firmware);

	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static void bus_acquire(ide_ident* cookie)
{
	if (cookie->dev->bus == 0)
		sem_acquire(ide_sem_0, 1);
	if (cookie->dev->bus == 1)
		sem_acquire(ide_sem_1, 1);
};

//--------------------------------------------------------------------------------
static void bus_release(ide_ident* cookie)
{
	if(cookie->dev->bus == 0)
		sem_release(ide_sem_0, 1);
	if(cookie->dev->bus == 1)
		sem_release(ide_sem_1, 1);
};

//--------------------------------------------------------------------------------
static int ide_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	ide_ident* cookie = (ide_ident*)_cookie;
	int err = 0;

	if(cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(cookie);

	switch(op) {
		case DISK_EXECUTE_DIAGNOSTIC:
			err = ata_execute_diagnostic(cookie->dev);
			break;

		case DISK_GET_GEOMETRY:
			err = ata_get_geometry(cookie->dev, buf, len);
			break;

		case DISK_USE_DMA:
		case DISK_USE_BUS_MASTERING:
			err = ERR_UNIMPLEMENTED;
			break;

		case DISK_USE_PIO:
			err = NO_ERROR;
			break;

		default:
			err = ERR_INVALID_ARGS;
	}

	bus_release(cookie);

	return err;
}

//--------------------------------------------------------------------------------
static ssize_t ide_read(dev_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
	ide_ident*	cookie = (ide_ident*)_cookie;
	uint32		sectors;
	uint32		currentSector;
	uint32		sectorsToRead;
	uint32		block;

	if (cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(cookie);

	// calculate start block and number of blocks to read
	block = pos / cookie->block_size;
	block += cookie->block_start;
	sectors = len / cookie->block_size;

	// correct len to be the actual # of bytes to read
	len -= len % cookie->block_size;

	// exit if read exceeds partition size
	if (block + sectors > cookie->block_start + cookie->block_count) {
		bus_release(cookie);
		return 0;
	}

	currentSector = 0;
	while(currentSector < sectors) {
		sectorsToRead = (sectors - currentSector) > 255 ? 255 : sectors;
		if (ata_read_block(cookie->dev, buf, block, sectorsToRead) != NO_ERROR) {
			bus_release(cookie);
			return ERR_IO_ERROR;
		}

		block += sectorsToRead * cookie->block_size;
		currentSector += sectorsToRead;
	}

	bus_release(cookie);

	return len;
}

//--------------------------------------------------------------------------------
static ssize_t ide_write(dev_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
	int			block;
	ide_ident*	cookie = _cookie;
	uint32		sectors;
	uint32		currentSector;
	uint32		sectorsToWrite;

	dprintf("ide_write: entry buf %p, pos 0x%Lx, *len %Ld\n", buf, pos, (long long)len);

	if(cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(cookie);

	block = pos / cookie->block_size + cookie->block_start;
	sectors = len / cookie->block_size;

	// exit if write exceeds partition size
	if (block + sectors > cookie->block_start + cookie->block_count) {
		bus_release(cookie);
		return ERR_INVALID_ARGS;
	}

	currentSector = 0;
	while(currentSector < sectors) {
		sectorsToWrite = (sectors - currentSector) > 255 ? 255 : sectors;

		if (ata_write_block(cookie->dev, buf, block, sectorsToWrite) != NO_ERROR) {
			//	  dprintf("ide_write: ide_block returned %d\n", rc);
			len = currentSector * cookie->block_size;
			bus_release(cookie);
			return ERR_IO_ERROR;
		}

		block += sectorsToWrite * cookie->block_size;
		currentSector += sectorsToWrite;
	}

	bus_release(cookie);

	return len;
}

//--------------------------------------------------------------------------------
static struct dev_calls ide_hooks = {
	ide_open,
	ide_close,
	ide_freecookie,

	NULL, // pointless to seek ide devices
	ide_ioctl,

	ide_read,
	ide_write,

	// can't page from ide devices
	NULL,
	NULL,
	NULL
};

//--------------------------------------------------------------------------------
static int ide_interrupt_handler(void* data)
{
  dprintf("in ide interrupt handler\n");

  return INT_RESCHEDULE;
}

//--------------------------------------------------------------------------------
static ide_ident* ide_create_device_ident(ide_device* dev, int16 partition)
{
	ide_ident* ident = kmalloc(sizeof(ide_ident));

	if (ident != NULL) {
		ident->dev = dev;
		ident->block_size = dev->bytes_per_sector;

		if (partition >= 0 && partition < MAX_PARTITIONS) {
			ident->block_start = dev->partitions[partition].starting_block;
			ident->block_count = dev->partitions[partition].sector_count;
		}
		else
		{
			ident->block_start = 0;
			ident->block_count = dev->sector_count;
		}
	}

	return ident;
}

//--------------------------------------------------------------------------------
static bool ide_attach_devices(int bus, int device)
{
	ide_device*  ide = &devices[(bus*2) + device];
	ide_ident*   ident = NULL;
	char         devpath[256];
	uint8        part, type;

	ide->bus    = bus;
	ide->device = device;
    type = ide_identify_device(bus, device);

	// register all hdd partitions with devfs
	if (type == ATA_DEVICE) {
		ide->magic1 = IDE_MAGIC_COOKIE1;
        ide->magic2 = IDE_MAGIC_COOKIE2;

		if (ata_get_partitions(ide)) {
			for (part=0; part < MAX_PARTITIONS; part++) {
				if (ide->partitions[part].partition_type != 0 &&
					(ident=ide_create_device_ident(ide, part)) != NULL) {
					sprintf(devpath, "disk/ata/%d/%d/%d", bus, device, part);
					devfs_publish_device(devpath, ident, &ide_hooks);
				}
			}

	dprintf("ide_attach_devices(bus=%d,dev=%d) rc=ata_device\n", bus, device);

			return true;
		}
	}

	// register cd/dvd drives with devfs
	// plugins handle partitions due to multitude of formats
	if (type == ATAPI_DEVICE) {
		sprintf(devpath, "disk/atapi/%d/%d", bus, device);
		devfs_publish_device(devpath, ident, &ide_hooks);

	dprintf("ide_attach_devices(bus=%d,dev=%d) rc=atapi_device\n", bus, device);

		return true;
	}

	dprintf("ide_attach_devices(bus=%d,dev=%d) rc=no_device\n", bus, device);

	return false;
}

//--------------------------------------------------------------------------------
static bool ide_attach_bus(int bus)
{
	uint8 dev, found = 0;

	for(dev=0; dev < 2; dev++)
		if (ide_attach_devices(bus, dev))
			found++;

	return(found > 0 ? true : false);
}

//--------------------------------------------------------------------------------
int ide_bus_init(kernel_args *ka)
{
	// create top-level semaphore for ide bus #1
	if ((ide_sem = sem_create(1, "ide_sem_0")) < 0)
		return ide_sem;

	// attach ide bus #1
	int_set_io_interrupt_handler(0x20 + IDE_0_INTERRUPT, &ide_interrupt_handler, NULL);
	ide_raw_init(0x1f0, 0x3f0);
	ide_attach_bus(0);

	// create top-level semaphore for ide bus #2
	if ((ide_sem = sem_create(1, "ide_sem_1")) < 0)
		return ide_sem;

	// attach ide bus #2
	int_set_io_interrupt_handler(0x20 + IDE_1_INTERRUPT, &ide_interrupt_handler, NULL);
	ide_raw_init(0x170, 0x370);
	ide_attach_bus(1);

	return NO_ERROR;
}
