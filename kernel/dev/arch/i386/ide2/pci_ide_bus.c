#include "ide_bus.h"
#include "pci_ide_bus.h"

#include "ide_drive.h"



#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/vfs.h>
#include <kernel/sem.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/bus/bus.h>
#include <kernel/bus/pci/pci_bus.h>
#include <nulibc/string.h>
#include <nulibc/stdarg.h>

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

#define BM_COMMAND_REG    0            // offset to command reg
#define BM_CR_MASK_READ    0x00           // read from memory
#define BM_CR_MASK_WRITE   0x08           // write to memory
#define BM_CR_MASK_START   0x01           // start transfer
#define BM_CR_MASK_STOP    0x00           // stop transfer

#define BM_STATUS_REG     2            // offset to status reg
#define BM_SR_MASK_SIMPLEX 0x80           // simplex only
#define BM_SR_MASK_DRV1    0x40           // drive 1 can do dma
#define BM_SR_MASK_DRV0    0x20           // drive 0 can do dma
#define BM_SR_MASK_INT     0x04           // INTRQ signal asserted
#define BM_SR_MASK_ERR     0x02           // error
#define BM_SR_MASK_ACT     0x01           // active

#define BM_PRD_ADDR_LOW   4            // offset to prd addr reg low 16 bits
#define BM_PRD_ADDR_HIGH  6            // offset to prd addr reg high 16 bits

typedef	struct
{
	uint32		address;
	uint16		count;
	uint8		reserved_1;
	uint8 		reserved_2 : 7;
	uint8 		eot : 1;
}prd_entry;
typedef	struct
{
	int		base_reg;
	int		status_reg;
	ide_drive	*drives[2];
	void		*drive_cookie[2];
	unsigned int	pio_reg_addrs[10];
	region_id	prd_region_id;
	int		channel;
	bool		dma_supported;
	sem_id		bus_semaphore;
	int		io_port;
	int		irq;
	bool		busMasterActivated;
	uint32		*prd_buf_address;
	addr		prd_phy_address;
	uint8		stat_reg;
	uint8		rw_control;
	uint8		*raw_buffer;
	uint8		*mapped_address;
	uint8		*current_dma;
	uint32		current_dma_length;
}pci_bus_cookie;
//#define IDE_TRACE 0

uint32		*tmp_buffer = (uint32*)0x80000;
#if IDE_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif
static int ide_interrupt_handler(void* data)
{
  TRACE(("in ide interrupt handler %p\n",data));
  pci_bus_cookie	*cookie = data;
  TRACE(("drive = %p drive_cookie=%p\n",cookie->drives[0],cookie->drive_cookie[0]));
  TRACE(("drive = %p drive_cookie=%p\n",cookie->drives[1],cookie->drive_cookie[1]));

	if(cookie->drives[0]!=NULL)
	{
		if ( ( in8( cookie->io_port  + BM_STATUS_REG )& BM_SR_MASK_INT ))                                // done yet ?
			cookie->drives[0]->signal_interrupt(cookie,cookie->drive_cookie[0]);
	}
	if(cookie->drives[1]!=NULL)
	{
		if ( ( in8( cookie->io_port  + BM_STATUS_REG )& BM_SR_MASK_INT ))                                // done yet ?
  			cookie->drives[1]->signal_interrupt(cookie,cookie->drive_cookie[1]);
  	}
  return  INT_RESCHEDULE;
}
extern	void	init_ide_struct(int bus,int device,int partition_id);

static	int	ide_raw_init(pci_bus_cookie *cookie)
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
#define	_INTEL_VENDORID		0x8086
#define	_82371AB_DEVICE_ID	0x7111

static	int	pci_init(pci_bus_cookie *cookie)
{
	id_list 	*vendor_ids;
	id_list 	*device_ids;
	int 		err;
	device 		dev;
	int 		i,j;
	int		fd;
	struct pci_cfg 	cfg;
	int		pp;

	vendor_ids = kmalloc(sizeof(id_list) + sizeof(uint32));
	vendor_ids->num_ids = 1;
	vendor_ids->id[0] = _INTEL_VENDORID;
	device_ids = kmalloc(sizeof(id_list) + sizeof(uint32));
	device_ids->num_ids = 1;
	device_ids->id[0] = _82371AB_DEVICE_ID;

	err = bus_find_device(1, vendor_ids, device_ids, &dev);
	if(err < 0)
	{
		TRACE(("unable to pci find controller\n"));
		err = -1;
		goto err;
	}
	fd = sys_open(dev.dev_path,STREAM_TYPE_DEVICE,0);
	sys_ioctl(fd, PCI_DUMP_CFG, NULL, 0);
	sys_ioctl(fd, PCI_GET_CFG, &cfg, sizeof(struct pci_cfg));
	sys_close(fd);

	cookie->io_port = dev.base[4];
	cookie->busMasterActivated = /*(buffer[0x9] & 0x80)!=0*/true;
	if(cookie->channel==0)
	{
		cookie->irq = 0x14;
		cookie->base_reg = 0x1F0;
		cookie->status_reg = 0x200 + cookie->base_reg;
	}
	else
	{
		cookie->irq = 0x15;
		cookie->base_reg = 0x170;
		cookie->status_reg = 0x200 + cookie->base_reg;
	}
	TRACE(("pci_init:irq=%X\n",cfg.irq));
	TRACE(("pci_init:header=%X\n",cfg.header_type));

	TRACE(("pci_init: found device at '%s'\n", dev.dev_path));
	for(i=0; i<MAX_DEV_IO_RANGES; i++)
	{
		TRACE(("pci_init: found device at base '%X'\n", dev.base[i]));
	}
	TRACE(("pci_init: found device at irq '%X'\n", dev.irq));

err:
	kfree(vendor_ids);
	kfree(device_ids);
	return err;
}
static	uint8	read_register(void *cookie,int reg);

static	int	init_dma(pci_bus_cookie *cookie)
{
	addr	temp;
	uint8	val = in8(cookie->io_port + BM_STATUS_REG );
	TRACE(("entering init_dma status = %d\n",val));
	if (!( val &( BM_SR_MASK_DRV1 | BM_SR_MASK_DRV0 )))
	{
      		TRACE(("BM_STATUS is wrong %d\n",val));
      		return -1;
      	}

	cookie->prd_region_id = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "ide_prd_buf", (void **)&cookie->prd_buf_address,
							REGION_ADDR_ANY_ADDRESS, 4096, REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL|LOCK_RW);


   	cookie->stat_reg = in8( cookie->io_port +  + BM_STATUS_REG );
   	cookie->stat_reg = cookie->stat_reg & 0xe0;

	memset(cookie->prd_buf_address, 0, 4096);

	vm_get_page_mapping(vm_get_kernel_aspace_id(), (addr)cookie->prd_buf_address, &cookie->prd_phy_address);
	cookie->raw_buffer = (uint8*)0x80000;
	vm_map_physical_memory(vm_get_kernel_aspace_id(), "test", (void *)&cookie->mapped_address, REGION_ADDR_ANY_ADDRESS,0x10000, LOCK_RW|LOCK_KERNEL, (addr)cookie->raw_buffer);
	TRACE(("mapped address is %X\n",cookie->mapped_address));
      	return 0;
}
static	int	init(int channel)
{
	char 		temp[64];
	pci_bus_cookie	*current_cookie = kmalloc(sizeof(pci_bus_cookie));
	current_cookie->channel = channel;
	pci_init(current_cookie);
	//return -1;
	bus_cookies[channel] = current_cookie;
	current_cookie->dma_supported = true;
	sprintf(temp, "pci_ide_sem%d", channel);
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
		init_dma(current_cookie);
		for(current_drive=0;current_drive<2;current_drive++)
		{
			int		i = 0;
			current_cookie->drives[current_drive] = NULL;
			TRACE(("trying to discover device %d/%d\n",channel,current_drive));
			while(ide_drives[i]!=NULL)
			{
				void	*drive_cookie = ide_drives[i]->init_drive(&pci_bus,current_cookie,channel,current_drive);
				if(drive_cookie!=NULL)
				{
					TRACE(("trying to discover device %d/%d is a %s\n",channel,current_drive,ide_drives[i]->device_name));
					current_cookie->drives[current_drive] = ide_drives[i];
					current_cookie->drive_cookie[current_drive] = drive_cookie;
					init_ide_struct(channel,current_drive,-1);
      					TRACE(("done discovering device\n"));
    					TRACE(("Cookie for interrupt is %p\n",current_cookie));
					int_set_io_interrupt_handler(0x20 + 14,&ide_interrupt_handler, current_cookie);
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
	pci_bus_cookie	*bus_cookie;
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
	pci_bus_cookie	*bus_cookie;
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
	TRACE(("entering setup_dma %x %d %d,%p\n",cookie,dir,size,buffer));

	pci_bus_cookie	*bus_cookie = cookie;
	prd_entry 	*table = (prd_entry*)bus_cookie->prd_buf_address;
	uint32		count = 0;
	uint32		physical_address;
	uint32		position = 0;
	uint32		bufaddr;
	uint8		*logical_address;

	out8( BM_CR_MASK_STOP,bus_cookie->io_port + BM_COMMAND_REG); // disable dma_channel
	out8( bus_cookie->stat_reg | BM_SR_MASK_INT | BM_SR_MASK_ERR,bus_cookie->io_port+ BM_STATUS_REG); // clear dma channel                              )
	bus_cookie->current_dma = buffer;
	bus_cookie->current_dma_length = size;
#define	PRD_ENTRIES	8
	while (size)
	{
		if (count++ >= PRD_ENTRIES)
		{
     			panic("DMA table too small\n");
		}
		else
		{
			physical_address = (uint32)(bus_cookie->raw_buffer + position);

			count = min (size, 0x10000);
			if ((physical_address & ~(0x10000-1)) != ((physical_address+count-1) & ~(0x10000-1)))
				count = 0x10000 - (physical_address & (0x10000-1));
			table->address = physical_address;
			table->count = count & 0xFFFFE;
			table->eot = 0;
			table++;
			size -= count;
			position += count;
		}
	}
	table--;
	table->eot = 1;
	uint32		reserved;

	reserved = in32(bus_cookie->io_port+BM_PRD_ADDR_LOW) & 0x03;
	out32(bus_cookie->prd_phy_address | reserved,bus_cookie->io_port+BM_PRD_ADDR_LOW);

	if ( dir )
	{
		bus_cookie->rw_control = BM_CR_MASK_READ;     // ATA Write DMA
	}
	else
	{
		bus_cookie->rw_control = BM_CR_MASK_WRITE;    // ATA Read DMA
	}
	out8( bus_cookie->rw_control,bus_cookie->io_port + BM_COMMAND_REG);
	return 0;
   }

static	int	start_dma(void *cookie)
{
	pci_bus_cookie	*bus_cookie = cookie;
	TRACE(("DMA about to start cookie=%p\n",bus_cookie));
	out8( bus_cookie->rw_control | BM_CR_MASK_START,bus_cookie->io_port + BM_COMMAND_REG );
	TRACE(("DMA should have started\n"));
	return 0;
}
static	int	finish_dma(void *cookie)
{
	pci_bus_cookie	*bus_cookie = cookie;
	uint8		status;

	TRACE(("waiting for DMA to finish\n"));
	out8( BM_CR_MASK_STOP,bus_cookie->io_port + BM_COMMAND_REG); // disable dma_channel
	status = read_register( cookie,CB_STAT );
	TRACE(("first status = %d\n",status));
	if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_DRQ | CB_STAT_ERR ) )
		return -1;
	status = in8( bus_cookie->io_port + BM_STATUS_REG );
	TRACE(("second status = %d %d\n",status,status & BM_SR_MASK_INT));

	TRACE(("pci_ide will copy %d bytes\n",bus_cookie->current_dma_length));
	bcopy(bus_cookie->mapped_address,bus_cookie->current_dma,bus_cookie->current_dma_length);
	switch(status & 0x5) {
		case 0:
				TRACE(("IDE -- finish_dma: DMA transfer failed\n"));
			return -1;
		case 1:
				TRACE(("IDE -- finish_dma: DMA transfer aborted\n"));
			return -1;
		case 4:
			return 0;
	    	case 5:
				TRACE(("IDE -- finish_dma: PRD size > device transfer size!\n"));
			return -1;
	}
	return 0;
}
static	int	write_register(void *cookie,int reg,uint8 value)
{
	pci_bus_cookie	*bus_cookie = cookie;
	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
	out8(value,reg_addr);
	return 0;
}
static	int	write_register16(void *cookie,int reg,uint16 value)
{
	pci_bus_cookie	*bus_cookie = cookie;
	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
	out16(value,reg_addr);
	return 0;
}
static	uint8	read_register(void *cookie,int reg)
{
	pci_bus_cookie	*bus_cookie = cookie;
  	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
  	return in8(reg_addr);
}
static	uint16	read_register16(void *cookie,int reg)
{
	pci_bus_cookie	*bus_cookie = cookie;
  	uint16	reg_addr = bus_cookie->pio_reg_addrs[reg];
  	return in16(reg_addr);
}
static	uint8	get_alt_value(void *cookie)
{
	return read_register( cookie,CB_ASTAT );
}
static	int	transfer_buffer(void *cookie,int reg,void *buffer,size_t size,bool fromMemory)
{
	pci_bus_cookie	*bus_cookie = cookie;
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
		pci_bus.write_register( cookie,CB_DH, CB_DH_DEV0 );
	else
		pci_bus.write_register( cookie,CB_DH, CB_DH_DEV1 );
	return 0;
}
static	int	delay_on_bus(void *cookie)
{
	pci_bus.get_alt_status(cookie); pci_bus.get_alt_status(cookie);
	pci_bus.get_alt_status(cookie); pci_bus.get_alt_status(cookie);
	return 0;
}
static	int	reset_bus(void *cookie,int default_drive)
{
  unsigned char	status;
  unsigned char devCtrl;
  devCtrl = CB_DC_HD15 | ( CB_DC_NIEN );

  pci_bus.write_register(cookie,CB_DC, devCtrl | CB_DC_SRST );
  pci_bus.delay_on_bus(cookie);
  pci_bus.write_register(cookie,CB_DC, devCtrl );
  pci_bus.delay_on_bus(cookie);
  if(pci_bus.wait_busy(cookie)==-1)
    return -1;
  pci_bus.select_drive( cookie,default_drive );
  pci_bus.delay_on_bus(cookie);
  return 0;
}
static	int	wait_busy(void *cookie)
{
	int		iterations = 0;
	uint8	status;

 	while ( 1 )
  	{
      		status = pci_bus.get_alt_status(cookie);
      		if ( ( status & CB_STAT_BSY ) == 0 )
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
	pci_bus_cookie	*bus_cookie = cookie;
	return bus_cookie->dma_supported;
}
static	int	lock(void *cookie)
{
	pci_bus_cookie	*bus_cookie = cookie;
	return sem_acquire(bus_cookie->bus_semaphore,1);
}
static	int	unlock(void *cookie)
{
	pci_bus_cookie	*bus_cookie = cookie;
	return sem_release(bus_cookie->bus_semaphore,1);
}
static	void *get_attached_drive(void *cookie,int drive)
{
	pci_bus_cookie	*bus_cookie = cookie;
	return	bus_cookie->drives[drive];
}
static	void 	*get_attached_drive_cookie(void *cookie,int drive)
{
	pci_bus_cookie	*bus_cookie = cookie;
	return	bus_cookie->drive_cookie[drive];
}
ide_bus	pci_bus =
{
	"PCI_BUS",
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
	&unlock
};
