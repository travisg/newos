#include "ide_bus.h"
#include "ata_ide_drive.h"

#include <kernel/dev/arch/i386/ide/ide_bus.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#define CB_DC_HD15   0x08  // bit should always be set to one
#define CB_DC_NIEN   0x02  // disable interrupts

#define CB_DH_DEV0 0xa0    // select device 0
#define CB_DH_DEV1 0xb0    // select device 1

#define DH_DRV1				0x1
#define DH_LBA				0x400

// Most mandtory and optional ATA commands (from ATA-3),

#define CMD_CFA_ERASE_SECTORS            0xC0
#define CMD_CFA_REQUEST_EXT_ERR_CODE     0x03
#define CMD_CFA_TRANSLATE_SECTOR         0x87
#define CMD_CFA_WRITE_MULTIPLE_WO_ERASE  0xCD
#define CMD_CFA_WRITE_SECTORS_WO_ERASE   0x38
#define CMD_CHECK_POWER_MODE1            0xE5
#define CMD_CHECK_POWER_MODE2            0x98
#define CMD_DEVICE_RESET                 0x08
#define CMD_EXECUTE_DEVICE_DIAGNOSTIC    0x90
#define CMD_FLUSH_CACHE                  0xE7
#define CMD_FORMAT_TRACK                 0x50
#define CMD_IDENTIFY_DEVICE              0xEC
#define CMD_IDENTIFY_DEVICE_PACKET       0xA1
#define CMD_IDENTIFY_PACKET_DEVICE       0xA1
#define CMD_IDLE1                        0xE3
#define CMD_IDLE2                        0x97
#define CMD_IDLE_IMMEDIATE1              0xE1
#define CMD_IDLE_IMMEDIATE2              0x95
#define CMD_INITIALIZE_DRIVE_PARAMETERS  0x91
#define CMD_INITIALIZE_DEVICE_PARAMETERS 0x91
#define CMD_NOP                          0x00
#define CMD_PACKET                       0xA0
#define CMD_READ_BUFFER                  0xE4
#define CMD_READ_DMA                     0xC8
#define CMD_READ_DMA_QUEUED              0xC7
#define CMD_READ_MULTIPLE                0xC4
#define CMD_READ_SECTORS                 0x20
#define CMD_READ_VERIFY_SECTORS          0x40
#define CMD_RECALIBRATE                  0x10
#define CMD_SEEK                         0x70
#define CMD_SET_FEATURES                 0xEF
#define CMD_SET_MULTIPLE_MODE            0xC6
#define CMD_SLEEP1                       0xE6
#define CMD_SLEEP2                       0x99
#define CMD_STANDBY1                     0xE2
#define CMD_STANDBY2                     0x96
#define CMD_STANDBY_IMMEDIATE1           0xE0
#define CMD_STANDBY_IMMEDIATE2           0x94
#define CMD_WRITE_BUFFER                 0xE8
#define CMD_WRITE_DMA                    0xCA
#define CMD_WRITE_DMA_QUEUED             0xCC
#define CMD_WRITE_MULTIPLE               0xC5
#define CMD_WRITE_SECTORS                0x30
#define CMD_WRITE_VERIFY                 0x3C



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

#define	NO_ERR	0
#define	ERR_EXPIRED_TIME_OUT	-1
#define	ERR_DISK_BUSY		-2
#define	ERR_HARDWARE_ERROR	-3
#define	ERR_DRQ_NOT_SET		-4
#define	ERR_TRANSMIT_BUFFER_NOT_EMPTY	-5
#define	ERR_DEVICE_FAULT	-6

#define	DRIVE_SUPPORT_LBA		0x0200

#define IDE_TRACE 1

#if IDE_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

#define	STATUS_WAITING_INTR	1
#define	STATUS_IDLE		2
typedef	struct	s_ata_command
{
	int			cmd;
	int			fr;
	int			sc;
	int			cyl;
	int			head;
	int			sect;
	uint8			*output;
	int			numSect;
	int			multiCnt;
	bool			read;
	bool			useInterrupt;
}ata_command;

typedef	struct
{
	ide_bus		*attached_bus;
	void		*attached_bus_cookie;
	int		position;
	bool		lba_supported;
	long		sector_count;
	uint16		bytes_per_sector;
	ide_drive_info	device_info;
	int		status;
	ata_command	*active_command;
	mutex		interrupt_mutex;
}ata_drive_cookie;
static void	ide_reg_poll(ide_bus *bus, void *b_cookie)
{
	uint8		status;
	while(1)
	{
		status = bus->get_alt_status(b_cookie);       // poll for not busy
		if ( ( status & CB_STAT_BSY ) == 0 )
    			break;
	}
}
static	int	convert_error(int error)
{
	if(error & CB_STAT_BSY)
		return ERR_DISK_BUSY;
	if(error & CB_STAT_DF)
		return ERR_DEVICE_FAULT;
	if(error & CB_STAT_ERR)
		return ERR_HARDWARE_ERROR;

	return 0;
}


static	void	signal_interrupt(void *b_cookie,void *drive_cookie)
{
	ata_drive_cookie	*cookie = drive_cookie;
	TRACE(("in signal interrupt status is %d\n",cookie->status));
	if(cookie->status == STATUS_WAITING_INTR)
	{
		TRACE(("unlock mutex\n"));
		mutex_unlock(&cookie->interrupt_mutex);
	}
}
int execute_command( ide_bus *bus, void *b_cookie,ata_drive_cookie *drive_cookie,ata_command *command)
{
	unsigned char 		devHead;
	unsigned char 		devCtrl;
	unsigned char 		cylLow;
	unsigned char 		cylHigh;
	unsigned char 		status;
	unsigned int 		wordCnt;
	int			i;
	uint16			*buffer = (uint16*)command->output;
	int			error_code;
	if(command->useInterrupt==true)
		devCtrl = CB_DC_HD15;
	else
		devCtrl = CB_DC_HD15 | CB_DC_NIEN;
	devHead = drive_cookie->position ? CB_DH_DEV1 : CB_DH_DEV0;
	devHead = devHead | ( command->head & 0x4f );
	cylLow = command->cyl & 0x00ff;
	cylHigh = ( command->cyl & 0xff00 ) >> 8;
	// these commands transfer only 1 sector
	if (( command->cmd == CMD_IDENTIFY_DEVICE )|| ( command->cmd == CMD_IDENTIFY_DEVICE_PACKET )|| ( command->cmd == CMD_READ_BUFFER )|| ( command->cmd == CMD_WRITE_BUFFER))
		command->numSect = 1;
	// only Read Multiple uses multiCnt
	if ( (command->cmd != CMD_READ_MULTIPLE ) && ( command->cmd != CMD_WRITE_MULTIPLE )   && ( command->cmd != CMD_CFA_WRITE_MULTIPLE_WO_ERASE ))
		command->multiCnt = 1;

	if(command->useInterrupt)
	{
		drive_cookie->active_command = command;
		drive_cookie->status = STATUS_WAITING_INTR;
	}
	if ( bus->select_drive( b_cookie,drive_cookie->position) )
	{
		return ERR_EXPIRED_TIME_OUT;
	}
	bus->write_register(b_cookie, CB_DC, devCtrl );
	bus->write_register(b_cookie, CB_FR, command->fr );
	bus->write_register(b_cookie, CB_SC, command->sc );
	bus->write_register(b_cookie, CB_SN, command->sect );
	bus->write_register(b_cookie, CB_CL, cylLow );
	bus->write_register(b_cookie, CB_CH, cylHigh );
	bus->write_register(b_cookie, CB_DH, devHead );

	bus->write_register(b_cookie, CB_CMD, command->cmd );

	if(command->useInterrupt)
	{
		TRACE(("locking interrupt mutex\n"));
		mutex_lock(&drive_cookie->interrupt_mutex);
		TRACE(("entering wait loop\n"));
	}
	else
	{
		bus->delay_on_bus(b_cookie);
	}
	while ( 1 )
	{
		if(command->read)
		{
			if(command->useInterrupt)
			{
				mutex_lock(&drive_cookie->interrupt_mutex);
				mutex_unlock(&drive_cookie->interrupt_mutex);
			}
			else
				ide_reg_poll(bus,b_cookie);
		}
		status = bus->get_alt_status(b_cookie);
		if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
		{
			wordCnt = command->multiCnt ? command->multiCnt : 1;
			if ( wordCnt > command->numSect )
				wordCnt = command->numSect;
			wordCnt = wordCnt * 256;
			bus->transfer_buffer( b_cookie,CB_DATA, buffer, wordCnt ,command->read);
			bus->delay_on_bus(b_cookie);
			command->numSect = command->numSect - ( command->multiCnt ? command->multiCnt : 1 );
			buffer+=wordCnt;
		}
		if(!command->read)
		{
			if(command->useInterrupt)
			{
				mutex_lock(&drive_cookie->interrupt_mutex);
				mutex_unlock(&drive_cookie->interrupt_mutex);
			}
			else
				ide_reg_poll(bus,b_cookie);
			status = bus->get_alt_status(b_cookie);
		}
		error_code = convert_error(status);
		if(error_code!=0)
			return error_code;
		if ( ( status & CB_STAT_DRQ ) == 0 )
		{
			return ERR_DRQ_NOT_SET;
		}
		if ( command->numSect < 1 )
		{
			status = bus->get_alt_status(b_cookie);
			error_code = convert_error(status);
			if(error_code!=0)
				return error_code;
			if( status & CB_STAT_DRQ)
			{
				return ERR_TRANSMIT_BUFFER_NOT_EMPTY;
			}
			break;   // go to READ_DONE
		}
	}
	drive_cookie->status = STATUS_IDLE;
	return NO_ERR;
}
int execute_command_dma( ide_bus *bus, void *b_cookie,ata_drive_cookie *drive_cookie,  ata_command *command)

{
	unsigned char 	devHead;
	unsigned char 	devCtrl;
	unsigned char 	cylLow;
	unsigned char 	cylHigh;
	unsigned char 	status;
	int 		ns;
	unsigned long 	lw1, lw2;
	int		doTwo;

	// setup register values

	dprintf("entering execute_command_dma %X %X %X %X %X\n",bus,b_cookie,drive_cookie,command,command->output);
	devCtrl = 0;
	devHead = drive_cookie->position ? CB_DH_DEV1 : CB_DH_DEV0;
	devHead = devHead | ( command->head & 0x4f );
	cylLow = command->cyl & 0x00ff;
	cylHigh = ( command->cyl & 0xff00 ) >> 8;

	// Quit now if the command is incorrect.

	if ( ( command->cmd != CMD_READ_DMA) && ( command->cmd != CMD_WRITE_DMA) )
	{
		return -1;
	}

	// set up the dma transfer(s)

	ns = command->sc;
	if ( ! ns )
		ns = 256;
	drive_cookie->active_command = command;
	doTwo = bus->setup_dma(b_cookie,command->cmd == CMD_WRITE_DMA, ns * 512L, command->output);

	if ( bus->select_drive( b_cookie,drive_cookie->position) )
	{
		return ERR_EXPIRED_TIME_OUT;
	}
	// Set up all the registers except the command register.
	bus->write_register(b_cookie, CB_DC, devCtrl );
	bus->write_register(b_cookie, CB_FR, command->fr );
	bus->write_register(b_cookie, CB_SC, command->sc );
	bus->write_register(b_cookie, CB_SN, command->sect );
	bus->write_register(b_cookie, CB_CL, cylLow );
	bus->write_register(b_cookie, CB_CH, cylHigh );
	bus->write_register(b_cookie, CB_DH, devHead );

	bus->write_register(b_cookie, CB_CMD, command->cmd );
	drive_cookie->status = STATUS_WAITING_INTR;
   	bus->delay_on_bus(b_cookie);
	bus->start_dma( b_cookie);
   	dprintf("first lock\n");
   	mutex_lock(&drive_cookie->interrupt_mutex);
   	dprintf("second lock\n");

   	mutex_lock(&drive_cookie->interrupt_mutex);
   	mutex_unlock(&drive_cookie->interrupt_mutex);
	return  bus->finish_dma(b_cookie);
}
static	void ide_string_conv (char *str, int len)
{
  unsigned int i;
  int	       j;
  for (i = 0; i < len / sizeof (unsigned short); i++)
    {
      char	c = str[i*2+1];
      str[i*2+1] = str[i*2];
      str[i*2] = c;
    }

  str[len - 1] = 0;
  for (j = len - 1; j >= 0 && str[j] == ' '; j--)
    str[j] = 0;
}

int		ide_identify_device (ide_bus *bus, void *b_cookie,ata_drive_cookie *drive_cookie,int bus_pos)
{
	int			ret;
	uint8			*buffer;
	ata_command		command;

	buffer = (uint8*)&drive_cookie->device_info;
	TRACE(("executing command\n"));
	command.cmd = CMD_IDENTIFY_DEVICE;
	command.fr = 1;
	command.sc = 0;
	command.cyl = 0;
	command.head = 0;
	command.sect = 0;
	command.output = buffer;
	command.numSect = 1;
	command.multiCnt = 0;
	command.read = true;
	command.useInterrupt = false;
	ret = execute_command(	bus,b_cookie,drive_cookie,&command);
	TRACE(("executing command %d\n",ret));
	if(ret<0)
		return	-1;
	ide_string_conv(drive_cookie->device_info.model,40);
	drive_cookie->sector_count = drive_cookie->device_info.cyls*drive_cookie->device_info.heads*drive_cookie->device_info.sectors;
	drive_cookie->bytes_per_sector = 512;
	drive_cookie->lba_supported = drive_cookie->device_info.capabilities & DRIVE_SUPPORT_LBA;

	TRACE( ("ide: disk at bus %d, device %d %s\n", bus_pos, drive_cookie->position,drive_cookie->device_info.model));
	TRACE(("ide/%d/%d: %dMB; %d cyl, %d head, %d sec, %d bytes/sec  (LBA=%d)\n",bus_pos, drive_cookie->position,
		drive_cookie->sector_count * drive_cookie->bytes_per_sector / 1024,
		drive_cookie->device_info.cyls,
		drive_cookie->device_info.heads,
		drive_cookie->device_info.sectors,
		drive_cookie->bytes_per_sector,
		drive_cookie->lba_supported
	));
	return 0;
}
static	void	*init_drive(ide_bus *bus,void *b_cookie,int channel,int drive)
{
	uint8			sc,sn,ch,cl,st;
  	unsigned char 		devCtrl = CB_DC_HD15 | CB_DC_NIEN ;
  	void			*ret = NULL;

  	bus->write_register(b_cookie,CB_DC,devCtrl);
	bus->select_drive(b_cookie,drive);
  	bus->delay_on_bus(b_cookie);

	bus->write_register( b_cookie,CB_SC, 0x55 );
	bus->write_register( b_cookie,CB_SN, 0xaa );
	bus->write_register( b_cookie,CB_SC, 0xaa );
	bus->write_register( b_cookie,CB_SN, 0x55 );
	bus->write_register( b_cookie,CB_SC, 0x55 );
	bus->write_register( b_cookie,CB_SN, 0xaa );
	sc = bus->read_register( b_cookie,CB_SC );
	sn = bus->read_register( b_cookie,CB_SN );
	if(sc!=0x55 && sn!=0xaa)
	{
		return NULL;
	}
	bus->select_drive(b_cookie,drive);
	bus->delay_on_bus(b_cookie);
	bus->reset_bus( b_cookie,drive);
	bus->select_drive(b_cookie,drive);
  	bus->delay_on_bus(b_cookie);
  	sc = bus->read_register( b_cookie,CB_SC );
  	sn = bus->read_register( b_cookie,CB_SN );
  	if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
    	{
    		ret = NULL;
      		cl = bus->read_register( b_cookie,CB_CL );
      		ch = bus->read_register( b_cookie,CB_CH );
      		st = bus->read_register( b_cookie,CB_STAT );
      		if ( ( cl == 0x14 ) && ( ch == 0xeb ) )
			ret = NULL;
      		else
			if ( ( cl == 0x00 ) && ( ch == 0x00 ) && ( st != 0x00 ) )
	  		{
	  			ata_drive_cookie *cookie = kmalloc(sizeof(ata_drive_cookie));
	  			cookie->attached_bus = bus;
	  			cookie->attached_bus_cookie = b_cookie;
	  			cookie->position = drive;
				mutex_init(&cookie->interrupt_mutex,"test");
	  			if(ide_identify_device(bus,b_cookie,cookie,channel)==0)
	  				ret = cookie;
	  			else
	  				ret = NULL;
	  		}
    	}
	return ret;
}
void ide_btochs(uint32 block, ata_drive_cookie *dev, int *cylinder, int *head, int *sect)
{
	uint32	sav_block = block;
  *sect = (block % dev->device_info.sectors) + 1;
  block /= dev->device_info.sectors;
  *head = (block % dev->device_info.heads) | (dev->position ? DH_DRV1 : 0);
  block /= dev->device_info.heads;
  *cylinder = block & 0xFFFF;
  TRACE(("ide_btochs: block %d -> cyl %d head %d sect %d\n", sav_block, *cylinder, *head, *sect));
}
void ide_btolba(uint32 block, ata_drive_cookie *dev, int *cylinder,int *head, int *sect)
{
	uint32	sav_block = block;
  *sect = block & 0xFF;
  *cylinder = (block >> 8) & 0xFFFF;
  *head = ((block >> 24) & 0xF) | ( dev->position ? DH_DRV1: 0) | DH_LBA;
  TRACE(("ide_btolba: block %d -> cyl %d head %d sect %d\n", sav_block, *cylinder, *head, *sect));
}
static	int	read_block(void *b_cookie,void *d_cookie,long block,void *buffer,size_t size)
{
	ata_drive_cookie	*drive_cookie = d_cookie;
	ide_bus			*bus = drive_cookie->attached_bus;
	int 			cyl, head, sect;
	ata_command		command;
	TRACE(("in read_blocks %x\n",block));
  	if(drive_cookie->lba_supported==1)
    		ide_btolba(block, drive_cookie, &cyl, &head, &sect);
  	else
   		ide_btochs(block, drive_cookie, &cyl, &head, &sect);

    	TRACE(("about to read from %d %d %d\n",cyl,head,sect));
    	command.fr = 0;
    	command.sc = size;
    	command.cyl = cyl;
    	command.head = head;
    	command.sect = sect;
    	command.output = buffer;
    	command.numSect = size;
    	command.multiCnt = 2;
    	command.read = true;
    	command.useInterrupt = true;
    	if(drive_cookie->attached_bus->support_dma(b_cookie)==true)
    	{
    		TRACE(("Will use DMA to transfer data\n"));
    		command.cmd = CMD_READ_DMA;
  		return execute_command_dma( bus,b_cookie,drive_cookie,&command);
	}
	else
	{
		TRACE(("Will use PIO to transfer data\n"));
		command.cmd = CMD_READ_SECTORS;
  		return execute_command( bus,b_cookie,drive_cookie,&command);
	}
}
static	int	write_block(void *b_cookie,void *d_cookie,long block,const void *buffer,size_t size)
{
	ata_drive_cookie	*drive_cookie = d_cookie;
	ide_bus			*bus = drive_cookie->attached_bus;
	int 			cyl, head, sect;
	ata_command		command;
  	if(drive_cookie->lba_supported==1)
    		ide_btolba(block, drive_cookie, &cyl, &head, &sect);
  	else
   		ide_btochs(block, drive_cookie, &cyl, &head, &sect);
       	command.fr = 0;
    	command.sc = size;
    	command.cyl = cyl;
    	command.head = head;
    	command.sect = sect;
    	command.output = buffer;
    	command.numSect = size;
    	command.multiCnt = 2;
    	command.read = false;
    	command.useInterrupt = true;

    	if(drive_cookie->attached_bus->support_dma(b_cookie)==true)
    	{
    		command.cmd = CMD_WRITE_DMA;
  		return execute_command_dma( bus,b_cookie,drive_cookie,&command);
	}
	else
	{
		command.cmd = CMD_WRITE_SECTORS;
  		return execute_command( bus,b_cookie,drive_cookie,&command);
	}

}
static	uint16		get_bytes_per_sector(void *d_cookie)
{
	ata_drive_cookie	*cookie = d_cookie;
	return	cookie->bytes_per_sector;
}
static	int		ioctl(void *b_cookie,void *d_cookie,int command,void *buffer,size_t size)
{
	ata_drive_cookie	*cookie = d_cookie;
	int			retCode;
	switch(command)
	{
		case	10000:
		{
			/*if(size!=sizeof(device_information))
			{
				retCode = -1;
			}
			else
			{
				device_information	*ret = (device_information*)buffer;
				ret->bytes_per_sector = cookie->bytes_per_sector;
				ret->sectors = cookie->sector_count;
				ret->heads = cookie->device_info.heads;
				ret->cylinders = cookie->device_info.cyls;
				retCode = 0;
			}*/
			break;
		}
		default:
		{
			retCode = -1;
		}
	}
	return retCode;

}
ide_drive	ata_ide_drive =
{
	"ATA_DRIVE",
	&init_drive,
	&read_block,
	&write_block,
	&get_bytes_per_sector,
	&ioctl,
	&signal_interrupt,
	NULL
};
