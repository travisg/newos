#include "ide_bus.h"
#include "isa_ide_bus.h"
#include "ide_drive.h"

#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <kernel/sem.h>
#include <kernel/int.h>
#include <kernel/vm.h>

#define CB_DC_HD15   0x08  // bit should always be set to one
#define CB_DC_SRST   0x04  // soft reset
#define CB_DC_NIEN   0x02  // disable interrupts

#define CB_DH_DEV0 0xa0    // select device 0
#define CB_DH_DEV1 0xb0    // select device 1

#define CB_STAT_BSY  0x80  // busy
#define CB_STAT_RDY  0x40  // ready
#define CB_STAT_DF   0x20  // device fault
#define CB_STAT_WFT  0x20  // write fault (old name)
#define CB_STAT_SKC  0x10  // seek complete
#define CB_STAT_SERV 0x10  // service
#define CB_STAT_DRQ  0x08  // data request
#define CB_STAT_CORR 0x04  // corrected
#define CB_STAT_IDX  0x02  // index
#define CB_STAT_ERR  0x01  // error (ATA)
#define CB_STAT_CHK  0x01  // check (ATAPI)

#define DMA_SEL5 0x01
#define DMA_SEL6 0x02
#define DMA_SEL7 0x03

#define DMA_TC5 0x02
#define DMA_TC6 0x04
#define DMA_TC7 0x08

#define DMA_MODE_DEMAND 0x00     // modeByte bits for various dma modes
#define DMA_MODE_BLOCK  0x80
#define DMA_MODE_SINGLE 0x40

#define DMA_MODE_MEMR 0x08       // modeByte memory read or write
#define DMA_MODE_MEMW 0x04

#define DMA_MASK_ENABLE  0x00    // bits for enable/disable
#define DMA_MASK_DISABLE 0x04

extern	void	init_ide_struct(int bus,int device,int partition_id);

#define IDE_TRACE 1

#if IDE_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

typedef	struct
{
	int		base_reg;
	int		status_reg;
	ide_drive	*drives[2];
	void		*drive_cookie[2];
	unsigned int 	pio_reg_addrs[10];
	int		channel;
	bool		dma_supported;
	sem_id		bus_semaphore;
}isa_bus_cookie;

static	int	ide_raw_init(isa_bus_cookie *cookie)
{
   cookie->pio_reg_addrs[ CB_DATA ] = cookie->base_reg + 0;  // 0
   cookie->pio_reg_addrs[ CB_FR   ] = cookie->base_reg + 1;  // 1
   cookie->pio_reg_addrs[ CB_SC   ] = cookie->base_reg + 2;  // 2
   cookie->pio_reg_addrs[ CB_SN   ] = cookie->base_reg + 3;  // 3
   cookie->pio_reg_addrs[ CB_CL   ] = cookie->base_reg + 4;  // 4
   cookie->pio_reg_addrs[ CB_CH   ] = cookie->base_reg + 5;  // 5
   cookie->pio_reg_addrs[ CB_DH   ] = cookie->base_reg + 6;  // 6
   cookie->pio_reg_addrs[ CB_CMD  ] = cookie->base_reg + 7;  // 7
   cookie->pio_reg_addrs[ CB_DC   ] = cookie->status_reg + 6;  // 8
   cookie->pio_reg_addrs[ CB_DA   ] = cookie->status_reg + 7;  // 9
   return 0;
}

static int ide_interrupt_handler(void* data)
{
  TRACE(("in ide interrupt handler %p\n",data));
  isa_bus_cookie	*cookie = data;
  TRACE(("drive = %p drive_cookie=%p\n",cookie->drives[0],cookie->drive_cookie[0]));
  TRACE(("drive = %p drive_cookie=%p\n",cookie->drives[1],cookie->drive_cookie[1]));

	if(cookie->drives[0]!=NULL)
		cookie->drives[0]->signal_interrupt(cookie,cookie->drive_cookie[0]);
	if(cookie->drives[1]!=NULL)
  		cookie->drives[1]->signal_interrupt(cookie,cookie->drive_cookie[1]);
  return  INT_RESCHEDULE;
}

static	int	init(int channel)
{
	char 		temp[64];
	isa_bus_cookie	*current_cookie = kmalloc(sizeof(isa_bus_cookie));

	bus_cookies[channel] = current_cookie;
	current_cookie->channel = channel;
	current_cookie->dma_supported = false;
	sprintf(temp, "isa_ide_sem%d", channel);
	current_cookie->bus_semaphore =  sem_create(1, temp);

    	if(current_cookie->bus_semaphore < 0)
      	{
		kfree(current_cookie);
		return -1;
      	}
	TRACE(("trying to discover devices on channel %d\n",channel));
	if(channel==0)
	{
		current_cookie->base_reg = 0x1f0;
		current_cookie->status_reg = 0x3f0;
	}
	else
	{
		current_cookie->base_reg = 0x170;
		current_cookie->status_reg = 0x370;
	}
	if(ide_raw_init(current_cookie)==0)
	{
		int	current_drive;
		for(current_drive=0;current_drive<2;current_drive++)
		{
			int		i = 0;
			current_cookie->drives[current_drive] = NULL;
			TRACE(("trying to discover device %d/%d\n",channel,current_drive));
			while(ide_drives[i]!=NULL)
			{
				void	*drive_cookie = ide_drives[i]->init_drive(&isa_bus,current_cookie,channel,current_drive);
				if(drive_cookie!=NULL)
				{
					TRACE(("trying to discover device %d/%d is a %s\n",channel,current_drive,ide_drives[i]->device_name));
					current_cookie->drives[current_drive] = ide_drives[i];
					current_cookie->drive_cookie[current_drive] = drive_cookie;
					init_ide_struct(channel,current_drive,-1);
      					TRACE(("done discovering device\n"));
    					TRACE(("Cookie for interrupt is %p\n",current_cookie));
					int_set_io_interrupt_handler(0x20 + 14+channel,&ide_interrupt_handler, current_cookie);
					break;
				}
				i++;
			}
		}
		return 0;
	}
	return -1;
}
static	int	read_block(void *cookie,int drive,long block,void *buffer,size_t size)
{
	isa_bus_cookie	*bus_cookie;
	ide_drive	*thedrive;
	if(drive<0 || drive>1)
		return -1;
	bus_cookie = cookie;
	thedrive = bus_cookie->drives[drive];
	if(thedrive==NULL)
		return -1;
	return thedrive->read_block(bus_cookie,bus_cookie->drive_cookie[drive],block,buffer,size);
}
static	int	write_block(void *cookie,int drive,long block,void *buffer,size_t size)
{
	isa_bus_cookie	*bus_cookie;
	ide_drive	*thedrive;
	if(drive<0 || drive>1)
		return -1;
	bus_cookie = cookie;
	thedrive = bus_cookie->drives[drive];
	if(thedrive==NULL)
		return -1;
	return thedrive->write_block(bus_cookie,bus_cookie->drive_cookie[drive],block,buffer,size);
}
static	int	setup_dma(void *cookie,int dir, long size, uint8 *buffer )
{
	return 	-1;
}

static	int	start_dma(void *cookie)
{
	return -1;
}
static	int	finish_dma(void *cookie)
{
	return -1;
}
static	int	write_register(void *cookie,int reg,uint8 value)
{
	isa_bus_cookie	*bus_cookie = cookie;
	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
	out8(value,reg_addr);
	return 0;
}
static	int	write_register16(void *cookie,int reg,uint16 value)
{
	isa_bus_cookie	*bus_cookie = cookie;
	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
	out16(value,reg_addr);
	return 0;
}
static	uint8	read_register(void *cookie,int reg)
{
	isa_bus_cookie	*bus_cookie = cookie;
  	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
  	return in8(reg_addr);
}
static	uint16	read_register16(void *cookie,int reg)
{
	isa_bus_cookie	*bus_cookie = cookie;
  	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
  	return in16(reg_addr);
}
static	uint8	get_alt_value(void *cookie)
{
	return read_register( cookie,CB_ASTAT );
}
static	int	transfer_buffer(void *cookie,int reg,void *buffer,size_t size,bool fromMemory)
{
	isa_bus_cookie	*bus_cookie = cookie;
  	uint16		reg_addr = bus_cookie->pio_reg_addrs[reg];
  	if(fromMemory==false)
  	{
  		__asm__ __volatile__
    		(
     			"rep ; outsw" : "=S" (buffer), "=c" (size) : "d" (reg_addr),"0" (buffer),"1" (size)
     		);
	}
 	else
 	{
  		__asm__ __volatile__
    		(
     			"rep ; insw" : "=D" (buffer), "=c" (size) : "d" (reg_addr),"0" (buffer),"1" (size)
     		);
	}
	return 0;
}
static	int	select_drive(void *cookie,int drive)
{
	if(drive==0)
		isa_bus.write_register( cookie,CB_DH, CB_DH_DEV0 );
	else
		isa_bus.write_register( cookie,CB_DH, CB_DH_DEV1 );
	return 0;
}
static	int	delay_on_bus(void *cookie)
{
	isa_bus.get_alt_status(cookie); isa_bus.get_alt_status(cookie);
	isa_bus.get_alt_status(cookie); isa_bus.get_alt_status(cookie);
	return 0;
}
static	int	reset_bus(void *cookie,int default_drive)
{
  unsigned char	status;
  unsigned char devCtrl;
  devCtrl = CB_DC_HD15 | ( CB_DC_NIEN );

  isa_bus.write_register(cookie,CB_DC, devCtrl | CB_DC_SRST );
  isa_bus.delay_on_bus(cookie);
  isa_bus.write_register(cookie,CB_DC, devCtrl );
  isa_bus.delay_on_bus(cookie);
  if(isa_bus.wait_busy(cookie)==-1)
    return -1;
  isa_bus.select_drive( cookie,default_drive );
  isa_bus.delay_on_bus(cookie);
  return 0;
}
static	int	wait_busy(void *cookie)
{
	int		iterations = 0;
	uint8	status;

 	while ( 1 )
  	{
      		status = isa_bus.get_alt_status(cookie);
      		if ( ( status & CB_STAT_BSY ) == 0 && ( status & CB_STAT_DRQ ) == 0)
         		return 0;
      		if ( iterations>10000 )
      		{
         		return -1;
      		}
     		iterations++;
  	}
  	return 0;
}
static	void	*get_nth_cookie(int channel)
{
	return	bus_cookies[channel];
}
static	bool	support_dma(void *cookie)
{
	isa_bus_cookie	*bus_cookie = cookie;
	return bus_cookie->dma_supported;
}
static	int	lock(void *cookie)
{
	isa_bus_cookie	*bus_cookie = cookie;
	return sem_acquire(bus_cookie->bus_semaphore,1);
}
static	int	unlock(void *cookie)
{
	isa_bus_cookie	*bus_cookie = cookie;
	return sem_release(bus_cookie->bus_semaphore,1);
}
static	void *get_attached_drive(void *cookie,int drive)
{
	isa_bus_cookie	*bus_cookie = cookie;
	return	bus_cookie->drives[drive];
}
static	void 	*get_attached_drive_cookie(void *cookie,int drive)
{
	isa_bus_cookie	*bus_cookie = cookie;
	return	bus_cookie->drive_cookie[drive];
}
ide_bus	isa_bus =
{
	"ISA_BUS",
	&init,
	&read_block,
	&write_block,
	&setup_dma,
	&start_dma,
	&finish_dma,
	&write_register16,
	&write_register,
	&read_register16,
	&read_register,
	&get_alt_value,
	&transfer_buffer,
	&select_drive,
	&reset_bus,
	&delay_on_bus,
	&wait_busy,
	&get_nth_cookie,
	&get_attached_drive,
	&get_attached_drive_cookie,
	&support_dma,
	&lock,
	&unlock,
};
