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
#include <kernel/vm.h>
#include <newos/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>


//#include <kernel/dev/arch/i386/ide/ide_bus.h>

#include <kernel/fs/devfs.h>

#include <string.h>
#include <stdio.h>

#include "ide_private.h"
#include "ide_raw.h"

ide_device      devices[MAX_IDE_DEVICES];
sem_id ide_sem, ide_sem_0, ide_sem_1;

#define IDE_0_INTERRUPT  14
#define IDE_1_INTERRUPT  15

struct ide_ident {
	ide_device* dev;

	// specs for whole disk or partition
	off_t       block_start;
	off_t       block_count;
	uint16      block_size;
	off_t       length;      // count * size

	// temporary work buffer
	char        work_buf[0]; // size of a block
};
typedef struct ide_ident ide_ident;

struct ide_cookie {
	ide_ident *ident;
	off_t pos;
};
typedef struct ide_cookie ide_cookie;

//--------------------------------------------------------------------------------
static int ide_open(dev_ident _ident, dev_cookie *_cookie)
{
	ide_ident* ident = (ide_ident*)_ident;
	ide_cookie *cookie;

	cookie = (ide_cookie *)kmalloc(sizeof(ide_cookie));
	if(!cookie)
		return ERR_NO_MEMORY;

	cookie->ident = ident;
	cookie->pos = 0;

	dprintf("ide_open: ident %p, start 0x%Lx, count 0x%Lx, size 0x%x\n", 
		ident, ident->block_start, ident->block_count, ident->block_size);

	*_cookie = cookie;

	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ide_close(dev_cookie _cookie)
{
	return NO_ERROR;
}

//--------------------------------------------------------------------------------
static int ide_freecookie(dev_cookie _cookie)
{
	ide_cookie *cookie = (ide_cookie *)_cookie;

	kfree(cookie);

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
static void bus_acquire(ide_ident* ident)
{
	if (ident->dev->bus == 0)
		sem_acquire(ide_sem_0, 1);
	if (ident->dev->bus == 1)
		sem_acquire(ide_sem_1, 1);
};

//--------------------------------------------------------------------------------
static void bus_release(ide_ident* ident)
{
	if(ident->dev->bus == 0)
		sem_release(ide_sem_0, 1);
	if(ident->dev->bus == 1)
		sem_release(ide_sem_1, 1);
};

//--------------------------------------------------------------------------------
static int ide_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	ide_cookie* cookie = (ide_cookie*)_cookie;
	int err = 0;

	if(cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(cookie->ident);

	switch(op) {
		case DISK_EXECUTE_DIAGNOSTIC:
			err = ata_execute_diagnostic(cookie->ident->dev);
			break;

		case DISK_GET_GEOMETRY:
			err = ata_get_geometry(cookie->ident->dev, buf, len);
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

	bus_release(cookie->ident);

	return err;
}

//--------------------------------------------------------------------------------
static ssize_t ide_read(dev_cookie _cookie, void *buf, off_t pos, ssize_t len)
{
	ide_cookie *cookie = (ide_cookie*)_cookie;
	ide_ident  *ident = cookie->ident;
	uint32		curr_sector;
	int         err;
	int         offset, copy_len;
	ssize_t     bytes_read = 0;

	dprintf("ide_read: cookie %p, entry buf %p, pos 0x%Lx, len %ld, cookie->pos 0x%Lx\n", 
		cookie, buf, pos, len, cookie->pos);
	dprintf("\tblock_start 0x%Lx, block_count 0x%Lx, block_size 0x%x\n",
		ident->block_start, ident->block_count, ident->block_size);

	if (cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(ident);

	// if pos is negative, use the current cookie position
	if(pos < 0)
		pos = cookie->pos;

	// make sure we're within the device
	if(pos >= ident->length) {
		bytes_read = 0;
		goto out;
	}

	// trim the read length if necessary
	if(pos + len > ident->length)
		len = ident->length - pos;
	
	// calculate the starting sector
	curr_sector = pos / ident->block_size;
	curr_sector += ident->block_start;

	// read the blocks into a temporary buffer, then copy into user space
	// XXX remove the intermediate read
	if(len > 0) {
		err = ata_read_sector(ident->dev, ident->work_buf, curr_sector, 1);
		if(err < 0) {
			bytes_read = err;
			goto out;
		}

		offset = pos % ident->block_size;
		copy_len = ident->block_size - offset;
		err = user_memcpy(buf, ident->work_buf + offset, copy_len); 
		if(err < 0) {
			bytes_read = err;
			goto out;
		}

		pos += copy_len;
		bytes_read += copy_len;
		len -= copy_len;
		curr_sector++;
	}

	cookie->pos = pos;

out:
	bus_release(cookie->ident);

	dprintf("ide_read: returning %ld\n", bytes_read);

	return bytes_read;
}

//--------------------------------------------------------------------------------
static ssize_t ide_write(dev_cookie _cookie, const void *buf, off_t pos, ssize_t len)
{
	int			block;
	ide_cookie*	cookie = (ide_cookie *)_cookie;
	uint32		sectors;
	uint32		currentSector;
	uint32		sectorsToWrite;

	dprintf("ide_write: cookie %p, entry buf %p, pos 0x%Lx, len %ld\n", cookie, buf, pos, len);

	if(cookie == NULL)
		return ERR_INVALID_ARGS;

	bus_acquire(cookie->ident);

	if(pos < 0)
		pos = cookie->pos;

	block = pos / cookie->ident->block_size + cookie->ident->block_start;
	sectors = len / cookie->ident->block_size;

	// exit if write exceeds partition size
	if (block + sectors > cookie->ident->block_start + cookie->ident->block_count) {
		bus_release(cookie->ident);
		return ERR_INVALID_ARGS;
	}

	currentSector = 0;
	while(currentSector < sectors) {
		sectorsToWrite = (sectors - currentSector) > 255 ? 255 : sectors;

		if (ata_write_sector(cookie->ident->dev, buf, block, sectorsToWrite) != NO_ERROR) {
			//  dprintf("ide_write: ide_block returned %d\n", rc);
			len = currentSector * cookie->ident->block_size;
			bus_release(cookie->ident);
			return ERR_IO_ERROR;
		}

		block += sectorsToWrite * cookie->ident->block_size;
		currentSector += sectorsToWrite;
		pos += sectorsToWrite * cookie->ident->block_size;
	}

	cookie->pos = pos;

	bus_release(cookie->ident);

	return len;
}

//--------------------------------------------------------------------------------
static int ide_seek(dev_cookie _cookie, off_t pos, seek_type st)
{
	ide_cookie *cookie = (ide_cookie *)_cookie;
	int err = 0;

	bus_acquire(cookie->ident);

	switch(st) {
		case _SEEK_END:
			pos += (cookie->ident->block_start + cookie->ident->block_count) * cookie->ident->block_size;
			goto _SEEK_SET;
		case _SEEK_CUR:
			pos += cookie->pos;
_SEEK_SET:
		case _SEEK_SET:
			if(pos < 0)
				pos = 0;
			if(pos > (cookie->ident->block_start + cookie->ident->block_count) * cookie->ident->block_size)
				pos = (cookie->ident->block_start + cookie->ident->block_count) * cookie->ident->block_size;
			cookie->pos = pos;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	bus_release(cookie->ident);

	return err;
}


//--------------------------------------------------------------------------------
static struct dev_calls ide_hooks = {
	ide_open,
	ide_close,
	ide_freecookie,

	ide_seek,
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
static ide_ident* ide_create_device_ident(ide_device* dev)
{
	ide_ident* ident = kmalloc(sizeof(ide_ident) + dev->bytes_per_sector);

	if (ident != NULL) {
		ident->dev = dev;
		ident->block_size = dev->bytes_per_sector;
		ident->block_start = 0;
		ident->block_count = dev->sector_count;
		ident->length = ident->block_count * ident->block_size;
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

#if 0
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
#endif
		// register a raw ide device
		if((ident = ide_create_device_ident(ide)) != NULL) {
			sprintf(devpath, "disk/ata/%d/%d/raw", bus, device);
			devfs_publish_device(devpath, ident, &ide_hooks);
			dprintf("ide_attach_devices(bus=%d,dev=%d) rc=ata_device\n", bus, device);
			dprintf("\tdev %p, ident %p, start 0x%Lx, count 0x%Lx, blocksize 0x%x\n", 
				ide, ident, ident->block_start, ident->block_count, ident->block_size);
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
int dev_bootstrap(void);

int dev_bootstrap(void)
{
	// create top-level semaphore for ide bus #1
	if ((ide_sem = sem_create(1, "ide_sem_0")) < 0)
		return ide_sem;

	// attach ide bus #1
	int_set_io_interrupt_handler(IDE_0_INTERRUPT, &ide_interrupt_handler, NULL, "ide0");
	ide_raw_init(0x1f0, 0x3f0);
	ide_attach_bus(0);

	// create top-level semaphore for ide bus #2
	if ((ide_sem = sem_create(1, "ide_sem_1")) < 0)
		return ide_sem;

	// attach ide bus #2
	int_set_io_interrupt_handler(IDE_1_INTERRUPT, &ide_interrupt_handler, NULL, "ide1");
	ide_raw_init(0x170, 0x370);
	ide_attach_bus(1);

	return NO_ERROR;
}
