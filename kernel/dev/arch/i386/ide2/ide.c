/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/fs/devfs.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>
#include <kernel/bus/bus.h>

#include <nulibc/string.h>
#include <nulibc/stdarg.h>


#include "ide_bus.h"
#include "ide_drive.h"
#include "isa_ide_bus.h"
#include "pci_ide_bus.h"


bool 	ide_get_partitions(int bus,int drive,ide_drive *device);
void	init_ide_struct(int bus,int device,int partition_id);
void	print_hex(uint8 value);
bool 	ide_get_partition_info(int bus,int device, partition_info *partition, uint32 position);
int 	ide_bus_init(kernel_args *ka);

struct ide_fs
{
	char 	 	*full_path;
	mutex 		lock;
};
sem_id ide_sem;
sem_id rw_sem;

typedef	struct
{
	ide_bus		*bus;
  	void		*bus_cookie;
  	ide_drive	*drive;
  	void		*drive_cookie;
  	uint16		bytes_per_sector;
  	mutex		lock;
  	char		*buffer;
  	int		partition_id;
  	long		start_block;
  	long		end_block;
}ide_cookie;

static int ide_open(dev_ident ident, dev_cookie *_cookie)
{
  int 		err;
  int		thebus;
  int		device;
  int		partition;
  ide_cookie	*cookie = NULL;
  struct ide_fs	* fs= (struct ide_fs*)ident;

  dprintf("ide_open: entry on '%s'(%d,%d)\n", fs->full_path,fs->full_path[8],fs->full_path[10]);

  mutex_lock(&fs->lock);
  thebus = (fs->full_path[8] - '0');
  if( thebus< 0 || thebus> 2)
    {
      err = -1;
      goto err;
    }
  device = (fs->full_path[10] - '0');
  if( device<0 || device> 2)
    {
      err = -1;
      goto err;
    }
  if(fs->full_path[12] != 'r')
  {
  	partition = fs->full_path[12] - '0';
  }
  else
  	partition = -1;
  cookie = kmalloc(sizeof(ide_cookie));
  if(cookie==NULL)
    {
      err = -1;
      goto err;
    }
  cookie->bus_cookie 	= bus_cookies[thebus];
  if(cookie->bus_cookie==NULL)
  {
  	dprintf("unable to get bus cookie\n");
  	err = -1;
  	goto err;
  }
  cookie->bus	     	= buses[thebus];
  if(cookie->bus==NULL)
  {
  	dprintf("unable to get bus\n");
  	err = -1;
  	goto err;
  }
  cookie->drive      	= cookie->bus->get_attached_drive(cookie->bus_cookie,device);
  cookie->drive_cookie  = cookie->bus->get_attached_drive_cookie(cookie->bus_cookie,device);
  cookie->bytes_per_sector = cookie->drive->get_bytes_per_sector(cookie->drive_cookie);
  cookie->buffer = kmalloc(cookie->bytes_per_sector);
  cookie->partition_id = partition;
  if(partition==-1)
  {
  	cookie->start_block = 0;
  	//cookie->end_block = cookie->drive->
  }
  else
  {

  	dprintf("drive opened is partition %d %d\n",partition,ide_drives[thebus*device]->partitions[partition].starting_block);
  	cookie->start_block = ide_drives[thebus*device]->partitions[partition].starting_block;
  	cookie->end_block = ide_drives[thebus*device]->partitions[partition].sector_count;

  }
  mutex_init(&cookie->lock, "ide_lock");
  *_cookie = cookie;
  err = 0;
  goto out;
 err:
  if(cookie!=NULL)
    kfree(cookie);
 out:
  mutex_unlock(&fs->lock);
  return err;
}

static int ide_seek(dev_cookie cookie, off_t pos, seek_type st)
{
  dprintf("ide_seek: entry\n");
  return -1;
}

static int ide_close(dev_cookie cookie)
{
  dprintf("ide_close: entry\n");
  if(cookie!=NULL)
    kfree(cookie);
  return 0;
}

static ssize_t ide_read(dev_cookie _cookie, void *buf, off_t _pos, ssize_t len)
{
  	long		block;
  	int		err = 0;
  	ide_cookie	*cookie = _cookie;
  	long		currentBlock;
  	long		nb_blocks;
  	char		*buffer = buf;
  	long		transfer_size;
  	long		transfer_blocks;
  	int		i;
  	dprintf("ide_read: entry buf %X, pos %X,*len %X cookie=%X\n", (uint32)buf, (uint32)_pos, (uint32)len,(uint32)cookie);
  	if(cookie==NULL)
  	{
  		len = -1;
  		goto err2;
  	}
  	block = _pos / cookie->bytes_per_sector;
  	nb_blocks = len / cookie->bytes_per_sector;
  	len = 0;
  	if(nb_blocks>128)
  		transfer_blocks = 128;
  	else
  		transfer_blocks = nb_blocks;
  	transfer_size = transfer_blocks*cookie->bytes_per_sector;
  	for(i=0;i<nb_blocks;i+=transfer_blocks)
  	{
  		if(cookie->drive->read_block(cookie->bus_cookie,cookie->drive_cookie,block+ cookie->start_block,buffer,transfer_blocks)!=0)
		{
	  		goto err;
		}
		buffer += transfer_size;
		block+=transfer_blocks;
		len += transfer_size;
  	}
 	out:
 	err:
  		mutex_unlock(&cookie->lock);
	err2:
  		return len;


}
static int ide_write(dev_cookie _cookie, const void *buf, off_t _pos, ssize_t len)
{
  	long		block;
  	int		err = 0;
  	ide_cookie	*cookie = _cookie;
  	long		currentBlock;
  	long		nb_blocks;
  	char		*buffer = (void*)buf;
  	int		i;
  	dprintf("ide_write: entry buf %X, pos %X, *len %X cookie=%X\n", (uint32)buf, (uint32)_pos, (uint32)len,(uint32)cookie);
  	if(cookie==NULL)
  	{
  		len = -1;
  		goto err2;
  	}
  	block = _pos / cookie->bytes_per_sector;
  	nb_blocks = len / cookie->bytes_per_sector;
  	len = 0;
  	for(i=0;i<nb_blocks;i++)
  	{
  		if(cookie->drive->write_block(cookie->bus_cookie,cookie->drive_cookie,block+ cookie->start_block,buffer,1)!=0)
		{
	  		goto err;
		}
		buffer += cookie->bytes_per_sector;
		block++;
		len += cookie->bytes_per_sector;
  	}
 	out:
 	err:
  		mutex_unlock(&cookie->lock);
	err2:
  		return len;
}
static int ide_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	ide_cookie		*cookie = _cookie;
	int			retCode;
	mutex_lock(&cookie->lock);
	retCode = cookie->drive->ioctl(cookie->bus_cookie,cookie->drive_cookie,op,buf,len);
	mutex_unlock(&cookie->lock);
  	return retCode;
}
static int ide_freecookie(dev_cookie _cookie)
{
	ide_cookie		*cookie = _cookie;
	//TODO free the cookie and its fields
	return 0;
}


struct dev_calls ide_hooks = {
	&ide_open,
	&ide_close,
	&ide_freecookie,
	&ide_seek,
	&ide_ioctl,
	&ide_read,
	&ide_write,
	/* cannot page from pci devices */
	NULL,
	NULL,
	NULL
};
void	init_ide_struct(int bus,int device,int partition_id)
{
	char			sTmp[256];
	struct ide_fs 		*fs;

	if(partition_id==-1)
		sprintf((char*)&sTmp, "bus/ide/%d/%d/raw", bus, device);
	else
		sprintf((char*)&sTmp, "bus/ide/%d/%d/%d", bus, device,partition_id);
	fs = (struct ide_fs*)kmalloc(sizeof(struct ide_fs));
	fs->full_path = kmalloc(strlen((char*)&sTmp)+1);
	strcpy(fs->full_path, (char*)&sTmp);
	mutex_init(&fs->lock, "idebus_lock");
	devfs_publish_device(fs->full_path, fs, &ide_hooks);
}
void	print_hex(uint8 value)
{
	static	char	hexNumber[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	dprintf("%c%c ",hexNumber[(value >> 4) & 0xF],hexNumber[value & 0xF]);
}
bool ide_get_partition_info(int bus,int device, partition_info *partition, uint32 position)
{
	char 		buffer[512];
	uint8* 		partitionBuffer = buffer;
	int		fd;
	char		tmp[256];
	int		i,j;

	sprintf((char*)&tmp,"/dev/bus/ide/%d/%d/raw",bus,device);
  	fd = sys_open(tmp,STREAM_TYPE_ANY,0);
  	if(fd==-1)
  	{
  		dprintf("unable to open device %s\n",tmp);
  		return false;
  	}
  	if(sys_read(fd, &buffer, 0, 512)==-1)
  	{
  		dprintf("unable to read partition table on %s\n",tmp);
  		return false;
  	}
  	sys_close(fd);
  	for(i=0;i<512;i+=16)
  	{
  		for(j=0;j<16;j++)
  		{
  			print_hex(buffer[i+j]);
  		}
  		dprintf("\n");
  	}
	// Check partition table signature
	if (partitionBuffer[PART_MAGIC_OFFSET] != PARTITION_MAGIC1 ||partitionBuffer[PART_MAGIC_OFFSET+1] != PARTITION_MAGIC2)
	{
		dprintf("partition table magic is incorrect %x %x\n",partitionBuffer[PART_MAGIC_OFFSET],partitionBuffer[PART_MAGIC_OFFSET+1]);
		return false;
	}
	memcpy(partition, partitionBuffer + PARTITION_OFFSET, sizeof(partition_info) * MAX_PARTITION_PER_DRIVE);
	dprintf("read ok\n");
	return true;
}
bool ide_get_partitions(int bus,int drive,ide_drive *device)
{
	int		i;
	partition_info	*tmp = &device->partitions[0];

	memset(tmp, 0, sizeof(partition_info) );
  	if(ide_get_partition_info(bus,drive,tmp, 0) == false)
    		return false;

  	dprintf("Primary Partition Table\n");
  	for (i = 0; i < MAX_PARTITION_PER_DRIVE; i++)
    	{
      		dprintf("  %d: flags:%x type:%x start:%d:%d:%d end:%d:%d:%d stblk:%d count:%d\n", i,
	  		device->partitions[i].boot_flags,
	  		device->partitions[i].partition_type,
			device->partitions[i].starting_head,
			device->partitions[i].starting_sector,
			device->partitions[i].starting_cylinder,
			device->partitions[i].ending_head,
			device->partitions[i].ending_sector,
			device->partitions[i].ending_cylinder,
			device->partitions[i].starting_block,
			device->partitions[i].sector_count);
			if(device->partitions[i].partition_type != 0)
			{
				init_ide_struct(bus,drive,i);
			}
    	}
  	if(device->partitions[1].partition_type == PTDosExtended)
    	{
      		int extOffset = device->partitions[1].starting_block;
      		if(ide_get_partition_info(bus,device, (partition_info*)&device->partitions[4], extOffset) == false)
        		return false;
      		dprintf("Extended Partition Table\n");
      		for (i=4; i<4+MAX_PARTITION_PER_DRIVE; i++)
        	{
          		device->partitions[i].starting_block += extOffset;
          		dprintf("  %d: flags:%x type:%x start:%d:%d:%d end:%d:%d:%d stblk:%d count:%d\n",i,
				device->partitions[i].boot_flags,
				device->partitions[i].partition_type,
				device->partitions[i].starting_head,
				device->partitions[i].starting_sector,
				device->partitions[i].starting_cylinder,
				device->partitions[i].ending_head,
				device->partitions[i].ending_sector,
				device->partitions[i].ending_cylinder,
				device->partitions[i].starting_block,
				device->partitions[i].sector_count);
        	}
    	}
  	return true;
}
int ide_bus_init(kernel_args *ka)
{
	char	buffer[512];
  dprintf("entering init ide_bus\n");
  bus_register_bus("/dev/bus/ide");

  pci_bus.init(0);
  //isa_bus.init(0);
  isa_bus.init(1);
  buses[0] = &pci_bus;
  buses[1] = &pci_bus;
  ide_get_partitions(0,0,ide_drives[0]);
  return 0;
}


