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
#include <sys/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <libc/string.h>
#include <libc/printf.h>

#include <dev/arch/i386/ide/ide_bus.h>
#include "ide_private.h"
#include "ide_raw.h"

ide_device		devices[MAX_DEVICES];


#define IDE_INTERRUPT	14
struct ide_fs 
{
  fs_id		id;
  sem_id	sem;
  void		*covered_vnode;
  void		*redir_vnode;
  int		root_vnode; // just a placeholder to return a pointer to
};

sem_id ide_sem;
sem_id rw_sem;
typedef	struct	
{
  int		bus;
  int		device;
  uint32	block_start;
  uint32	partition_size;
  uint16	bytes_per_sector;
}ide_cookie;

static int ide_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
  struct ide_fs *fs = _fs;
  int 		err;
  int		bus;
  int		device;
  int		partition;
  ide_cookie	*cookie = NULL;
  dprintf("ide_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);
  
  sem_acquire(fs->sem, 1);
  if(fs->redir_vnode != NULL)
    {
      // we were mounted on top of
      redir->redir = true;
      redir->vnode = fs->redir_vnode;
      redir->path = path;
      err = 0;
      goto out;		
    }		
  if(stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE) 
    {
      err = ERR_VFS_PATH_NOT_FOUND;
      goto err;
    }
  bus = (path[0] - '0');
  if( bus< 0 || bus> 2)
    {
      err = ERR_VFS_PATH_NOT_FOUND;
      goto err;
    }
  device = (path[2] - '0');
  if( device<0 || device> 2)
    {
      err = ERR_VFS_PATH_NOT_FOUND;
      goto err;
    }
  if(devices[(bus*2) + device].magic!=IDE_MAGIC_COOKIE)
    {
      err = ERR_VFS_PATH_NOT_FOUND;
      goto err;
    }
  cookie = kmalloc(sizeof(ide_cookie));
  if(cookie==NULL)
    {
      err = ERR_NO_MEMORY;
      goto err;
    }
  cookie->bus = bus;
  cookie->device = device;
  cookie->bytes_per_sector = devices[(bus*2) + device].bytes_per_sector;
  partition = (path[4] - '0');  
  if(partition>='0' && partition<='9')
    {
      cookie->block_start = devices[(bus*2) + device].partitions[partition].starting_block;
      cookie->partition_size = devices[(bus*2) + device].partitions[partition].sector_count;
    }
  else
    {
      cookie->block_start = 0;
      cookie->partition_size = devices[(bus*2) + device].sector_count;
    }
  *_vnode = &fs->root_vnode;	
  *_cookie = cookie;
  err = 0;
  goto out;
 err:
  if(cookie!=NULL)
    kfree(cookie);
 out:
  sem_release(fs->sem, 1);
  return err;
}

static int ide_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
  dprintf("ide_seek: entry\n");
  return ERR_NOT_ALLOWED;
}

static int ide_close(void *_fs, void *_vnode, void *_cookie)
{
  dprintf("ide_close: entry\n");
  if(_cookie!=NULL)
    kfree(_cookie);
  return 0;
}

static int ide_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
  struct ide_fs *fs = _fs;
  int		ret = ERR_NOT_ALLOWED;
  dprintf("ide_create: entry %X\n",fs);
  if(fs->redir_vnode != NULL) {
    // we were mounted on top of
    redir->redir = true;
    redir->vnode = fs->redir_vnode;
    redir->path = path;
    ret = 0;
    goto out;
  }
	
 out:
  dprintf("ide_create: exit %X\n",fs);	
  return ret;
}

static int ide_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
  struct ide_fs *fs = _fs;
  int		ret;
  dprintf("ide_stat: entry %X\n",fs);
  if(fs->redir_vnode != NULL) {
    // we were mounted on top of
    redir->redir = true;
    redir->vnode = fs->redir_vnode;
    redir->path = path;
    ret = 0;
    goto out;
  }

  stat->size = 0;
  ret = 0;
	
 out:
  dprintf("ide_stat: exit %X\n",fs);	
  return ret;
}

static int ide_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
  int			block;
  int			err = 0;
  struct ide_fs 	*fs = _fs;
  ide_cookie		*cookie = _cookie;
  int			sectors;
  int			currentSector;
  int			sectorsToRead;
  dprintf("ide_read: entry buf 0x%x, pos 0x%x 0x%x, *len 0x%x\n", buf, pos, *len);
  sem_acquire(fs->sem, 1);
  if(cookie==NULL)
    {
      *len = 0;
      err = ERR_INVALID_ARGS;
      goto err;
    }
  block = pos / cookie->bytes_per_sector;
  block += cookie->block_start;
  sectors = *len / cookie->bytes_per_sector;

  if(block + sectors>cookie->block_start + cookie->partition_size)
    {
      *len = 0;
      err = 0;
      goto err;
    }
  currentSector = 0;
  while(currentSector < sectors)
    {

      sectorsToRead = (sectors - currentSector) > 255 ? 255 : sectors;
//   	dprintf("block 0x%x, sectorsToRead 0x%x\n", block, sectorsToRead);
      if(ide_read_block(&devices[(2*cookie->bus) + cookie->device] ,buf,block,sectorsToRead)!=0)
	{
	  *len = currentSector * cookie->bytes_per_sector;
	  err = ERR_IO_ERROR;
	  goto err;
	}
      block += sectorsToRead *  cookie->bytes_per_sector;
      currentSector += sectorsToRead;
    }
 out:
 err:
  sem_release(fs->sem, 1);

  return err;
}

static int ide_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
  int		block;
  int		err = 0;
  struct ide_fs *fs = _fs;
  ide_cookie	*cookie = _cookie;
  int		sectors;
  int		currentSector;
  int		sectorsToWrite;
  int       rc;

  dprintf("ide_write: entry buf 0x%x, pos 0x%x 0x%x, *len 0x%x\n", buf, pos, *len);
  sem_acquire(fs->sem, 1);
  if(cookie==NULL)
    {
      *len = 0;
      err = ERR_INVALID_ARGS;
      goto err;
    }
  block = pos / cookie->bytes_per_sector;
  block+= cookie->block_start;
  sectors = *len / cookie->bytes_per_sector;
  if(block + sectors>cookie->block_start + cookie->partition_size)
    {
      *len = 0;
      err = 0;
      goto err;
    }
  currentSector = 0;
  while(currentSector < sectors)
    {
      sectorsToWrite = (sectors - currentSector) > 255 ? 255 : sectors;
//   	dprintf("block 0x%x, sectorsToWrite 0x%x\n", block, sectorsToWrite); 
      rc = ide_write_block(&devices[(2*cookie->bus) + cookie->device],buf,block,sectorsToWrite);
      if(rc!=0)
	{
//	  dprintf("ide_write: ide_write_block returned %d\n", rc);
	  *len = currentSector * cookie->bytes_per_sector;
	  err = ERR_IO_ERROR;
	  goto err;
	}
      block += sectorsToWrite *  cookie->bytes_per_sector;
      currentSector += sectorsToWrite;
    }
 out:
 err:
  sem_release(fs->sem, 1);

  return err;
}
static int ide_get_geometry(void *_cookie,void *buf,size_t len)
{
  ide_cookie		*cookie = _cookie;
  ide_device		*device = &devices[(2*cookie->bus) + cookie->device];
  drive_geometry	*drive_geometry = buf;
  if(len<sizeof(drive_geometry))
    return ERR_VFS_INSUFFICIENT_BUF;

  drive_geometry->blocks = device->end_block - device->start_block;
  drive_geometry->heads = device->hardware_device.heads;
  drive_geometry->cylinders = device->hardware_device.cyls;
  drive_geometry->sectors = device->hardware_device.sectors;
  drive_geometry->removable = false;
  drive_geometry->bytes_per_sector = 512;
  drive_geometry->read_only = false;
  strcpy((char*)&drive_geometry->model,(char*)&device->hardware_device.model);
  strcpy((char*)&drive_geometry->serial,(char*)&device->hardware_device.serial);
  strcpy((char*)&drive_geometry->firmware,(char*)&device->hardware_device.firmware);

  return 0;
}
static int ide_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
  int err = 0;
  struct ide_fs *fs = _fs;
  sem_acquire(fs->sem,1);
  switch(op) 
    {
    case DISK_GET_GEOMETRY:
      {
	err = ide_get_geometry(_cookie,buf,len);
	break;
      }
    case DISK_USE_DMA:
      {
	err = 0;
	break;
      }
    case DISK_USE_BUS_MASTERING:
      {
	err = 0;
	break;
      }
    case DISK_USE_PIO:
      {
	err = 0;
	break;
      }

      break;
    default:
      err = ERR_INVALID_ARGS;
    } 
  sem_release(fs->sem, 1);
  return err;
}

static int ide_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
  struct ide_fs 		*fs;
  int err;
  
  fs = kmalloc(sizeof(struct ide_fs));
  if(fs == NULL)
    {
      err = ERR_NO_MEMORY;
      goto err;
    }
  
  fs->covered_vnode = covered_vnode;
  fs->redir_vnode = NULL;
  fs->id = id;
  
  {
    char temp[64];
    sprintf(temp, "ide_sem%d", id);
    
    fs->sem = sem_create(1, temp);
    if(fs->sem < 0)
      {
	err = ERR_NO_MEMORY;
	goto err1;
      }
  }
  
  *root_vnode = (void *)&fs->root_vnode;
  *fs_cookie = fs;
  
  return 0;
  
 err1:	
  kfree(fs);
err:
  return err;
}

static int ide_unmount(void *_fs)
{
  struct ide_fs *fs = _fs;

  sem_delete(fs->sem);
  kfree(fs);
  
  return 0;	
}

static int ide_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
  struct ide_fs *fs = _fs;
  
  fs->redir_vnode = redir_vnode;
  
  return 0;
}

static int ide_unregister_mountpoint(void *_fs, void *_v)
{
  struct ide_fs *fs = _fs;
  
  fs->redir_vnode = NULL;
  
  return 0;
}

static int ide_dispose_vnode(void *_fs, void *_v)
{
  return 0;
}

struct fs_calls ide_hooks = {
	&ide_mount,
	&ide_unmount,
	&ide_register_mountpoint,
	&ide_unregister_mountpoint,
	&ide_dispose_vnode,
	&ide_open,
	&ide_seek,
	&ide_read,
	&ide_write,
	&ide_ioctl,
	&ide_close,
	&ide_create,
	&ide_stat,
};

static	int	ide_interrupt_handler()
{
  dprintf("in ide interrupt Handler\n");
  //  sem_release(rw_sem,1);
  //sem_acquire(rw_sem,1);
  return  INT_RESCHEDULE;
}
static	int	ide_attach_device(int bus,int device)
{
  char		sTmp[256];
  int		i;
  devices[(bus*2) + device].bus = bus;
  devices[(bus*2) + device].device = device;
  if(!ide_identify_device(bus,device))
    {
      devices[(bus*2) + device].magic = IDE_MAGIC_COOKIE;
      devices[(bus*2) + device].magic2 = IDE_MAGIC_COOKIE2;
      sprintf((char *)&sTmp,"/dev/bus/ide/%d/%d/raw",bus,device);
      sys_create((char *)&sTmp, "", STREAM_TYPE_DEVICE);
      if(!ide_get_partitions(&devices[(bus*2) + device]))
	{
	  for(i=0;i<PARTITION_TBL_SIZE*2;i++)
	    {
	      if(devices[(bus*2) + device].partitions[i].partition_type!=0)
		{
		  sprintf((char *)&sTmp,"/dev/bus/ide/%d/%d/%d",bus,device,i);
		  sys_create((char *)&sTmp, "", STREAM_TYPE_DEVICE);
		}
	    }
	  return 0;
	}
      else
	return -1;
    }
  return -1;
}
static	char	getHexChar(uint8 value)
{
  if(value<10)
    return value + '0';
  return 'A' + (value - 10);
}
static	void	dumpHexLine(uint8 *buffer,int numberPerLine)
{
  uint8	*copy = buffer;
  int	i;
  for(i=0;i<numberPerLine;i++)
    {
      uint8	value1 = getHexChar(((*copy) >> 4));
      uint8	value2 = getHexChar(((*copy) & 0xF));
      
      dprintf("%c%c ",value1,value2);
      copy++;
    }
  copy = buffer;
  for(i=0;i<numberPerLine;i++)
    {
      if(*copy>=' ' && *copy<='Z')
	  dprintf("%c",*copy);
      else
	dprintf(".");
      copy++;
    }
  dprintf("\n");
}
void	dumpHexBuffer(uint8 *buffer,int size)
{
  int	numberPerLine = 8;
  int	numberOfLines = size / numberPerLine;
  int	i,j;
  for(i=0;i<numberOfLines;i++)
    {
      dprintf("%d ",i*numberPerLine);
      dumpHexLine(buffer,numberPerLine);
      buffer += numberPerLine;
    }
  
}
static	int	ide_attach_buses()
{
  int			i,j;
  int			found = 0;
  char		sTmp[256];
  for(i=0;i<1;i++)
    {
      sprintf((char *)&sTmp,"/dev/bus/ide/%d",i);
      sys_create((char *)&sTmp, "", STREAM_TYPE_DIR);
      for(j=0;j<2;j++)
  	{
	  sprintf((char *)&sTmp,"/dev/bus/ide/%d/%d",i,j);
	  sys_create((char *)&sTmp, "", STREAM_TYPE_DIR);
	  if(!ide_attach_device(i,j))
	    {
	      sprintf((char *)&sTmp,"/dev/bus/ide/%d/%d/raw",i,j);
	      sys_create((char *)&sTmp, "", STREAM_TYPE_DEVICE);
	      found++;
	    }
  	}
    }
  if(found>0)
    return 0;
  return -1;
}
int ide_bus_init(kernel_args *ka)
{
  int		i;
  int_set_io_interrupt_handler(0x20 + IDE_INTERRUPT,&ide_interrupt_handler);
  ide_raw_init(0x1f0,0x3f0);
  // create device node
  rw_sem = sem_create(1,"ide-rw");
  //sem_acquire(rw_sem,1);
  vfs_register_filesystem("ide_bus_fs", &ide_hooks);
  sys_create("/dev/bus", "", STREAM_TYPE_DIR);
  sys_create("/dev/bus/ide", "", STREAM_TYPE_DIR);
  sys_mount("/dev/bus/ide", "ide_bus_fs");
  ide_attach_buses();
  #if 0
  {
    int		len = 128*512;
    int		fd = sys_open("/dev/bus/ide/0/0/raw","",STREAM_TYPE_DEVICE);
    char	*block = (char*)kmalloc(len);
    if(block==NULL)
      panic("unable to allocate test buffer");

    for(i=0;i<len;i++)
      block[i] = i % 256;
    dprintf("IDE FD is %d\n",fd);
    if(fd!=-1)
      {
	if(sys_read(fd,block,0,&len)!=0)
	  dprintf("error while writing \n");
	sys_close(fd);
	dprintf("read 1 sector done\n");
      }
    kfree(block);
  }
  #endif
  return 0;
}


