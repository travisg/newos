#include "ide_bus.h"
#include "atapi_ide_drive.h"
#include <kernel/heap.h>
#include <kernel/net/misc.h>

#define CB_DC_HD15   0x08  // bit should always be set to one
#define CB_DC_NIEN   0x02  // disable interrupts

#define CB_DH_DEV0 0xa0    // select device 0
#define CB_DH_DEV1 0xb0    // select device 1
#define CMD_PACKET                       0xA0

/* ATAPI commands */
#define ATAPI_TEST_UNIT_READY		0x00	/* check if device is ready */
#define ATAPI_REZERO			0x01	/* rewind */
#define ATAPI_REQUEST_SENSE		0x03	/* get sense data */
#define ATAPI_FORMAT			0x04	/* format unit */
#define ATAPI_READ			0x08	/* read data */
#define ATAPI_WRITE			0x0a	/* write data */
#define ATAPI_WEOF			0x10	/* write filemark */
#define	    WF_WRITE				0x01
#define ATAPI_SPACE			0x11	/* space command */
#define	    SP_FM				0x01
#define	    SP_EOD				0x03
#define ATAPI_MODE_SELECT		0x15	/* mode select */
#define ATAPI_ERASE			0x19	/* erase */
#define ATAPI_MODE_SENSE		0x1a	/* mode sense */
#define ATAPI_START_STOP		0x1b	/* start/stop unit */
#define	    SS_LOAD				0x01
#define	    SS_RETENSION			0x02
#define	    SS_EJECT				0x04
#define ATAPI_PREVENT_ALLOW		0x1e	/* media removal */
#define ATAPI_READ_CAPACITY		0x25	/* get volume capacity */
#define ATAPI_READ_BIG			0x28	/* read data */
#define ATAPI_WRITE_BIG			0x2a	/* write data */
#define ATAPI_LOCATE			0x2b	/* locate to position */
#define ATAPI_READ_POSITION		0x34	/* read position */
#define ATAPI_SYNCHRONIZE_CACHE		0x35	/* flush buf, close channel */
#define ATAPI_WRITE_BUFFER		0x3b	/* write device buffer */
#define ATAPI_READ_BUFFER		0x3c	/* read device buffer */
#define ATAPI_READ_SUBCHANNEL		0x42	/* get subchannel info */
#define ATAPI_READ_TOC			0x43	/* get table of contents */
#define ATAPI_PLAY_10			0x45	/* play by lba */
#define ATAPI_PLAY_MSF			0x47	/* play by MSF address */
#define ATAPI_PLAY_TRACK		0x48	/* play by track number */
#define ATAPI_PAUSE			0x4b	/* pause audio operation */
#define ATAPI_READ_DISK_INFO		0x51	/* get disk info structure */
#define ATAPI_READ_TRACK_INFO		0x52	/* get track info structure */
#define ATAPI_RESERVE_TRACK		0x53	/* reserve track */
#define ATAPI_SEND_OPC_INFO		0x54	/* send OPC structurek */
#define ATAPI_MODE_SELECT_BIG		0x55	/* set device parameters */
#define ATAPI_REPAIR_TRACK		0x58	/* repair track */
#define ATAPI_READ_MASTER_CUE		0x59	/* read master CUE info */
#define ATAPI_MODE_SENSE_BIG		0x5a	/* get device parameters */
#define ATAPI_CLOSE_TRACK		0x5b	/* close track/session */
#define ATAPI_READ_BUFFER_CAPACITY	0x5c	/* get buffer capicity */
#define ATAPI_SEND_CUE_SHEET		0x5d	/* send CUE sheet */
#define ATAPI_BLANK			0xa1	/* blank the media */
#define ATAPI_SEND_KEY			0xa3	/* send DVD key structure */
#define ATAPI_REPORT_KEY		0xa4	/* get DVD key structure */
#define ATAPI_PLAY_12			0xa5	/* play by lba */
#define ATAPI_LOAD_UNLOAD		0xa6	/* changer control command */
#define ATAPI_READ_STRUCTURE		0xad	/* get DVD structure */
#define ATAPI_PLAY_CD			0xb4	/* universal play command */
#define ATAPI_SET_SPEED			0xbb	/* set drive speed */
#define ATAPI_MECH_STATUS		0xbd	/* get changer status */
#define ATAPI_READ_CD			0xbe	/* read data */
#define ATAPI_POLL_DSC			0xff	/* poll DSC status bit */

#define	ATAPI_CDROM_CAP_PAGE		0x2A
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

#define CB_SC_P_TAG    0xf8   // ATAPI tag (mask)
#define CB_SC_P_REL    0x04   // ATAPI release
#define CB_SC_P_IO     0x02   // ATAPI I/O
#define CB_SC_P_CD     0x01   // ATAPI C/D

#if IDE_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

typedef	struct
{
	ide_bus				*attached_bus;
	void				*attached_bus_cookie;
	int				position;
	struct atapi_capabilities_page	capabilities;
	struct atapi_toc		toc;
	int				block_size;
	long				disk_size;

}atapi_drive_cookie;

//extern	int		ide_identify_device (ide_bus *bus, void *b_cookie,atapi_drive_cookie *drive_cookie,int bus_pos,int command);
static int atapi_send_packet_read(void *b_cookie,void *d_cookie, uint8 *packet, uint8 *response, int responsel);


static int	atapi_mode_sense(void *b_cookie,void *d_cookie, int page, uint8  *pagebuf, int pagesize)
{
    	uint8 ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, page, 0, 0, 0, 0,pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };
    	return atapi_send_packet_read(b_cookie,d_cookie, ccb, pagebuf, pagesize);
}
static	int	atapi_test_ready(void *b_cookie,void *d_cookie)
{
	uint8 ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	return atapi_send_packet_read(b_cookie,d_cookie, ccb, NULL, 0);
}
void	bzero(uint8 *buffer,size_t size)
{
	size_t	i;
	for(i=0;i<size;i++)
	{
		buffer[i] = 0;
	}
}
static	int	atapi_read_toc(void *b_cookie,void *d_cookie,struct atapi_toc *toc)
{
	atapi_drive_cookie	*cookie = d_cookie;
	int 	track, ntracks, len;
    	uint8 	ccb[16];
	bzero(ccb, sizeof(ccb));

	len = sizeof(struct atapi_toc_header) + sizeof(struct atapi_toc_entry);
    	ccb[0] = ATAPI_READ_TOC;
    	ccb[7] = len>>8;
    	ccb[8] = len;
    	if(atapi_send_packet_read(b_cookie,d_cookie, ccb, (uint8*)toc, len)<0)
    	{
    		return -1;
    	}
    	ntracks = toc->hdr.last_track - toc->hdr.first_track + 1;
    	if (ntracks <= 0 || ntracks > MAX_TRACKS)
    	{
    		TRACE(("cdrom has invalid number of tracks %d\n",ntracks));
		return -1;
    	}
    	TRACE(("cdrom has %d tracks\n",ntracks));
    	len = sizeof(struct atapi_toc_header)+(ntracks+1)*sizeof(struct atapi_toc_entry);
    	bzero(ccb, sizeof(ccb));
    	ccb[0] = ATAPI_READ_TOC;
    	ccb[7] = len>>8;
    	ccb[8] = len;
    	if(atapi_send_packet_read(b_cookie,d_cookie, ccb, (uint8*)toc, len)<0)
    	{
    		return -1;
    	}
    	cookie->block_size = (toc->ent[0].control & 4) ? 2048 : 2352;
    	cookie->disk_size = ntohl(toc->ent[toc->hdr.last_track].addr.lba) ;
    	TRACE(("block size is %d\n", cookie->block_size));
    	TRACE(("Disk size is %d\n", cookie->disk_size* cookie->block_size));
	return 0;
}
static int atapi_send_packet_read(void *b_cookie,void *d_cookie, uint8 *packet, uint8 *response, int responsel)
{
	atapi_drive_cookie	*drive_cookie = d_cookie;
    	int 			i = 6;
    	uint16 			*ptr = (uint16 *)packet;
    	int 			rc = -1;
    	int 			maxl = responsel;
	int			devHead;
	int			devCtrl;
	int			count = 0;
	int			retry_count = 1000;
	uint8			status;
	uint8			reason;
	uint8			lowCyl;
	uint8			highCyl;
	uint8			cylLow = maxl & 0xFF;
	uint8			cylHigh = maxl >> 8;
	bigtime_t			timer;

	TRACE(("selecting driver\n"));
	drive_cookie->attached_bus->select_drive(b_cookie,drive_cookie->position);
	TRACE(("Drive is ready\n"));
  	devHead = drive_cookie->position ? CB_DH_DEV1 : CB_DH_DEV0;
  	devCtrl = CB_DC_HD15 | CB_DC_NIEN;
  	drive_cookie->attached_bus->write_register(b_cookie, CB_DC, devCtrl );
  	drive_cookie->attached_bus->write_register(b_cookie,CB_FR,0);
  	drive_cookie->attached_bus->write_register(b_cookie,CB_SC,0);
  	drive_cookie->attached_bus->write_register(b_cookie,CB_SN,0);
     	drive_cookie->attached_bus->write_register(b_cookie,CB_CH,maxl >> 8);
     	drive_cookie->attached_bus->write_register(b_cookie,CB_CL,maxl & 0xFF);
	drive_cookie->attached_bus->write_register(b_cookie, CB_DH, devHead );



	drive_cookie->attached_bus->write_register(b_cookie, CB_CMD, CMD_PACKET );

    	drive_cookie->attached_bus->delay_on_bus(b_cookie);
	status = drive_cookie->attached_bus->get_alt_status(b_cookie);
   	if ( status & CB_STAT_BSY )
   	{
      		// BSY=1 is OK
   	}
   	else
   	{
      		if ( status & ( CB_STAT_DRQ | CB_STAT_ERR ) )
      		{
         		// BSY=0 and DRQ=1 is OK
         		// BSY=0 and ERR=1 is OK
      		}
      		else
      		{
      			TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
      			drive_cookie->attached_bus->reset_bus(b_cookie,0);
      			return -1;
      		}
   	}
	count = 0;
	while(count<1000)
	{
		status = drive_cookie->attached_bus->get_alt_status(b_cookie);
		if ( ( status & CB_STAT_BSY ) == 0 )
			break;
		count++;
	}
	if(count==1000)
	{
		TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
      		drive_cookie->attached_bus->reset_bus(b_cookie,0);
      		return -1;
	}
	status = drive_cookie->attached_bus->get_alt_status(b_cookie);
	reason = drive_cookie->attached_bus->read_register(b_cookie,CB_SC);
	lowCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CL);
	highCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CH);
	if (( status & ( CB_STAT_BSY | CB_STAT_DRQ | CB_STAT_ERR ) )!= CB_STAT_DRQ)
	{
		TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
		drive_cookie->attached_bus->reset_bus(b_cookie,0);
		return -1;
	}
	if ( ( reason &  ( CB_SC_P_TAG | CB_SC_P_REL | CB_SC_P_IO ) )|| ( ! ( reason &  CB_SC_P_CD ) ))
	{
		TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
		drive_cookie->attached_bus->reset_bus(b_cookie,0);
		return -1;
	}
        if ( ( lowCyl != cylLow ) || ( highCyl != cylHigh ) )
	{
		TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
		drive_cookie->attached_bus->reset_bus(b_cookie,0);
		return -1;
	}

    	while (i--)
    	{
    		//drive_cookie->attached_bus->get_alt_status(b_cookie);
    		drive_cookie->attached_bus->write_register16(b_cookie, CB_DATA, *ptr++ );
    	}
    	ptr = (uint16 *)response;
    	drive_cookie->attached_bus->delay_on_bus(b_cookie);
    	TRACE(("b4 main loop\n"));
	while(1)
	{
		do
		{
			status = drive_cookie->attached_bus->read_register(b_cookie,CB_STAT);
		}while(status ==CB_STAT_BSY);
		reason = drive_cookie->attached_bus->read_register(b_cookie,CB_SC);
		lowCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CL);
		highCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CH);
		if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == 0 )
		{
			TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
			drive_cookie->attached_bus->reset_bus(b_cookie,0);
			return -1;
		}
		if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) != CB_STAT_DRQ )
		{
			TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
			drive_cookie->attached_bus->reset_bus(b_cookie,0);
			return -1;
		}
		if ( ( reason &  ( CB_SC_P_TAG | CB_SC_P_REL ) )|| ( reason &  CB_SC_P_CD ))
		{
			TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
			drive_cookie->attached_bus->reset_bus(b_cookie,0);
			return -1;
		}
		i = (highCyl << 8) | lowCyl;
		if (i > 0 && i <= maxl)
    		{
			i/=2;
			drive_cookie->attached_bus->transfer_buffer(b_cookie,CB_DATA,ptr,i,true);
			break;
    		}
    		else
    		{
			TRACE(("bus is in an inconsistent state resetting %s %d\n",__FILE__,__LINE__));
			drive_cookie->attached_bus->reset_bus(b_cookie,0);
			return -1;
    		}
    		drive_cookie->attached_bus->delay_on_bus(b_cookie);
	}
	status = drive_cookie->attached_bus->get_alt_status(b_cookie);
	reason = drive_cookie->attached_bus->read_register(b_cookie,CB_SC);
	lowCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CL);
	highCyl = drive_cookie->attached_bus->read_register(b_cookie,CB_CH);

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
		{
 			atapi_drive_cookie *cookie = kmalloc(sizeof(atapi_drive_cookie));
  			cookie->attached_bus = bus;
  			cookie->attached_bus_cookie = b_cookie;
  			cookie->position = drive;
  			atapi_test_ready(b_cookie,cookie);
  			atapi_mode_sense(b_cookie,cookie,ATAPI_CDROM_CAP_PAGE,(uint8*)&cookie->capabilities,sizeof(cookie->capabilities));
 			TRACE(("current drive speed is %d\n",cookie->capabilities.curspeed));
  			TRACE(("Drive support eject %d\n",cookie->capabilities.eject));
  			atapi_test_ready(b_cookie,cookie);
  			atapi_read_toc(b_cookie,cookie,&cookie->toc);
  			ret = cookie;
		}
      		else
			if ( ( cl == 0x00 ) && ( ch == 0x00 ) && ( st != 0x00 ) )
	  		{
	  			return NULL;
	  		}
    	}
	return ret;
}
static	int	read_block(void *b_cookie,void *d_cookie,long block,void *buffer,size_t size)
{
	atapi_drive_cookie	*cookie = d_cookie;
	ide_bus			*bus = cookie->attached_bus;
	uint8			ccb[16];
	int32 			lba, lastlba, count;
    	int 			track, blocksize;
	int			ret;

	TRACE(("trying to see if drive is ok\n"));
	atapi_test_ready(b_cookie,d_cookie);
	TRACE(("done trying to see if drive is ok\n"));
	dprintf("will read block %d\n",block);
	bzero(ccb, sizeof(ccb));
    	lba = block;
    	track = 0;
    	blocksize = cookie->block_size;
	lastlba = cookie->disk_size;
	count = size;
  	switch (cookie->block_size)
  	{
		case 2048:
	    		ccb[0] = ATAPI_READ_BIG;
	    		break;

		case 2352:
	    		ccb[0] = ATAPI_READ_CD;
	    		ccb[9] = 0xf8;
	    		break;

		default:
	    		ccb[0] = ATAPI_READ_CD;
	    		ccb[9] = 0x10;
	}
	ccb[1] = 0;
    	ccb[2] = lba>>24;
    	ccb[3] = lba>>16;
    	ccb[4] = lba>>8;
    	ccb[5] = lba;
    	ccb[6] = count>>16;
    	ccb[7] = count>>8;
    	ccb[8] = count;
	return atapi_send_packet_read(b_cookie,d_cookie, (uint8*)&ccb, (uint8*)buffer, count * blocksize);
}
static	int	write_block(void *b_cookie,void *d_cookie,long block,const void *buffer,size_t size)
{
	return	-1;
}
static	uint16		get_bytes_per_sector(void *d_cookie)
{
	atapi_drive_cookie	*cookie = d_cookie;
	return	cookie->block_size;
}
static	int		ioctl(void *b_cookie,void *d_cookie,int command,void *buffer,size_t size)
{
	switch(command)
	{
		// Eject device
		case	1:
		{
			uint8 ccb[16] = { ATAPI_START_STOP, 0, 0, 0, 3,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

			return 0;
		}
	}
	return -1;
}
static	void	signal_interrupt(void *b_cookie,void *drive_cookie)
{
}
ide_drive	atapi_ide_drive =
{
	"ATAPI_DRIVE",
	&init_drive,
	&read_block,
	&write_block,
	&get_bytes_per_sector,
	&ioctl,
	signal_interrupt
};
