#include "ide_private.h"
#include <kernel/arch/cpu.h>
#include <kernel/sem.h>
#include <kernel/debug.h>
#include "ide_raw.h"
#include "partition.h"
#include <libc/string.h>


// Global defines -- ATA register and register bits.
//
// command block & control block regs
//
// these are the offsets into pio_reg_addrs[]

#define CB_DATA  0   // data reg         in/out pio_base_addr1+0
#define CB_ERR   1   // error            in     pio_base_addr1+1
#define CB_FR    1   // feature reg         out pio_base_addr1+1
#define CB_SC    2   // sector count     in/out pio_base_addr1+2
#define CB_SN    3   // sector number    in/out pio_base_addr1+3
#define CB_CL    4   // cylinder low     in/out pio_base_addr1+4
#define CB_CH    5   // cylinder high    in/out pio_base_addr1+5
#define CB_DH    6   // device head      in/out pio_base_addr1+6
#define CB_STAT  7   // primary status   in     pio_base_addr1+7
#define CB_CMD   7   // command             out pio_base_addr1+7
#define CB_ASTAT 8   // alternate status in     pio_base_addr2+6
#define CB_DC    8   // device control      out pio_base_addr2+6
#define CB_DA    9   // device address   in     pio_base_addr2+7

// error reg (CB_ERR) bits

#define CB_ER_ICRC 0x80    // ATA Ultra DMA bad CRC
#define CB_ER_BBK  0x80    // ATA bad block
#define CB_ER_UNC  0x40    // ATA uncorrected error
#define CB_ER_MC   0x20    // ATA media change
#define CB_ER_IDNF 0x10    // ATA id not found
#define CB_ER_MCR  0x08    // ATA media change request
#define CB_ER_ABRT 0x04    // ATA command aborted
#define CB_ER_NTK0 0x02    // ATA track 0 not found
#define CB_ER_NDAM 0x01    // ATA address mark not found

#define CB_ER_P_SNSKEY 0xf0   // ATAPI sense key (mask)
#define CB_ER_P_MCR    0x08   // ATAPI Media Change Request
#define CB_ER_P_ABRT   0x04   // ATAPI command abort
#define CB_ER_P_EOM    0x02   // ATAPI End of Media
#define CB_ER_P_ILI    0x01   // ATAPI Illegal Length Indication

// ATAPI Interrupt Reason bits in the Sector Count reg (CB_SC)

#define CB_SC_P_TAG    0xf8   // ATAPI tag (mask)
#define CB_SC_P_REL    0x04   // ATAPI release
#define CB_SC_P_IO     0x02   // ATAPI I/O
#define CB_SC_P_CD     0x01   // ATAPI C/D

// bits 7-4 of the device/head (CB_DH) reg

#define CB_DH_DEV0 0xa0    // select device 0
#define CB_DH_DEV1 0xb0    // select device 1

// status reg (CB_STAT and CB_ASTAT) bits

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

// device control reg (CB_DC) bits

#define CB_DC_HD15   0x08  // bit should always be set to one
#define CB_DC_SRST   0x04  // soft reset
#define CB_DC_NIEN   0x02  // disable interrupts

//**************************************************************

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

#define	DRIVE_SUPPORT_LBA		0x0200
#define DH_DRV1				0x0
#define DH_DRV1				0x1
#define DH_LBA				0x400

#define DELAY400NS  { pio_inbyte( CB_ASTAT ); pio_inbyte( CB_ASTAT );  \
                      pio_inbyte( CB_ASTAT ); pio_inbyte( CB_ASTAT ); }
extern	sem_id	rw_sem;

unsigned int pio_base_addr1 = 0x1f0;
unsigned int pio_base_addr2 = 0x3f0;
unsigned int pio_memory_seg = 0;

unsigned int pio_reg_addrs[10];

unsigned char pio_last_write[10];
unsigned char pio_last_read[10];

static uint8	pio_inbyte(uint16 port)
{
  uint16	reg_addr = pio_reg_addrs[port];
  return in8(reg_addr);
}
static uint16	pio_inword(uint16 port)
{
  uint16	reg_addr = pio_reg_addrs[port];
  return in16(reg_addr);
}
static void	pio_rep_inword(uint16 port,uint16 *addr,unsigned long count)
{
  uint16	reg_addr = pio_reg_addrs[port];
  __asm__ __volatile__ 
    (
     "rep ; insw" : "=D" (addr), "=c" (count) : "d" (reg_addr),"0" (addr),"1" (count)
     );
}
static void	pio_rep_outword(uint16 port,uint16 *addr,unsigned long count)
{
  uint16	reg_addr = pio_reg_addrs[port];
  __asm__ __volatile__ 
    (
     "rep ; outsw" : "=S" (addr), "=c" (count) : "d" (reg_addr),"0" (addr),"1" (count)
     );
}
static void	pio_outbyte(uint16 port,uint8 data)
{
	uint16	reg_addr = pio_reg_addrs[port];
	out8(data,reg_addr);
	
}
static void	ide_reg_poll()
{
	uint8		status;
	while(1)
	{
		status = pio_inbyte( CB_ASTAT );       // poll for not busy
		if ( ( status & CB_STAT_BSY ) == 0 )
    	break;
	}
}
static	int	ide_wait_busy()
{
	int		iterations = 0;
	uint8	status;
	
 	while ( 1 )
  {
      status = pio_inbyte( CB_ASTAT );
      if ( ( status & CB_STAT_BSY ) == 0 )
         return NO_ERR;
      if ( iterations>10000 )
      {
         return 1;
      }
     	iterations++;
  }
}

static	int	ide_select_device(int bus,int device)
{
	uint8		status;
	int			iterations = 0;
	if(devices[(bus*2) + device].device_type < ATA_DEVICE)
	{
			pio_outbyte( CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0 );
      DELAY400NS;
      return NO_ERR;
	}
	if(ide_wait_busy())
		return ERR_EXPIRED_TIME_OUT;
	pio_outbyte( CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0 );
  DELAY400NS;
	while ( 1 )
  {
  	status = pio_inbyte( CB_STAT );
    if ( devices[(bus*2) + device].device_type == ATA_DEVICE )
    {
           if ( ( status & ( CB_STAT_BSY | CB_STAT_RDY | CB_STAT_SKC ) )
                     == ( CB_STAT_RDY | CB_STAT_SKC ) )
         break;
		}
    else
    {
    	if ( ( status & CB_STAT_BSY ) == 0 )
            break;
    }
    if(iterations>10000)
    	return ERR_EXPIRED_TIME_OUT;
    iterations++;
  }
  return NO_ERR;
}
static	void	ide_delay(int bus,int device)
{
  int	i;
  if(devices[(bus*2) + device].device_type != ATAPI_DEVICE)
    {
      return;
    }
  //  for(i=0;i<100000;i++);
  thread_snooze(1000000);
  //sys_snooze(110);
  //sleep(110);
  
}
int ide_base (int bus)
{
    if (bus == 0)
        return IDE_PRI_BASE;
    else if (bus == 1)
        return IDE_SEC_BASE;
    else
        return -1;
}


static int reg_pio_data_in( int bus,int dev, int cmd,int fr, int sc,unsigned int cyl, int head, int sect,uint8 *output,unsigned int numSect, int multiCnt )
{
  unsigned char devHead;
  unsigned char devCtrl;
  unsigned char cylLow;
  unsigned char cylHigh;
  unsigned char status;
  unsigned int wordCnt;
  int	i;
  uint16				*buffer = (uint16*)output;

//  dprintf("reg_pio_data_in: bus %d dev %d cmd %d fr %d sc %d cyl %d head %d sect %d numSect %d multiCnt %d\n",
//  	bus, dev, cmd, fr, sc, cyl, head, sect, numSect, multiCnt);

  devCtrl = CB_DC_HD15 | CB_DC_NIEN;
  devHead = dev ? CB_DH_DEV1 : CB_DH_DEV0;
  devHead = devHead | ( head & 0x4f );
  cylLow = cyl & 0x00ff;
  cylHigh = ( cyl & 0xff00 ) >> 8;
  // these commands transfer only 1 sector
  if (    ( cmd == CMD_IDENTIFY_DEVICE )
	  || ( cmd == CMD_IDENTIFY_DEVICE_PACKET )
	  || ( cmd == CMD_READ_BUFFER )
	  )
    numSect = 1;
  // only Read Multiple uses multiCnt
  if ( cmd != CMD_READ_MULTIPLE )
    multiCnt = 1;

  //numSect = 1;
  // Reset error return data.


  // Select the drive - call the sub_select function.
  // Quit now if this fails.
  if ( ide_select_device( bus,dev ) )
    {
      return ERR_EXPIRED_TIME_OUT;
    }
  // Set up all the registers except the command register.

  pio_outbyte( CB_DC, devCtrl );
  pio_outbyte( CB_FR, fr );
  pio_outbyte( CB_SC, sc );
  pio_outbyte( CB_SN, sect );
  pio_outbyte( CB_CL, cylLow );
  pio_outbyte( CB_CH, cylHigh );
  pio_outbyte( CB_DH, devHead );

  pio_outbyte( CB_CMD, cmd );
  DELAY400NS;
  while ( 1 )
    {
      ide_delay( bus,dev );
      ide_reg_poll();
      status = pio_inbyte( CB_STAT );
      if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
	{
	  wordCnt = multiCnt ? multiCnt : 1;
	  if ( wordCnt > numSect )
            wordCnt = numSect;
	  wordCnt = wordCnt * 256;
	  pio_rep_inword( CB_DATA, buffer, wordCnt );
	  DELAY400NS;    
	  numSect = numSect - ( multiCnt ? multiCnt : 1 );
	  buffer+=wordCnt;
	}
      if( status & CB_STAT_BSY)
	{
	  return ERR_DISK_BUSY;
	}
      if( status & CB_STAT_DF)
	{
	  return ERR_DEVICE_FAULT;
	}
      if(status & CB_STAT_ERR)
	{
	  return ERR_HARDWARE_ERROR;
	}
      if ( ( status & CB_STAT_DRQ ) == 0 )
	{
	  return ERR_DRQ_NOT_SET;
	}
      if ( numSect < 1 )
 	{
	  ide_delay( bus,dev );
	  status = pio_inbyte( CB_STAT );
	  if( status & CB_STAT_BSY)
	    {
	      return ERR_DISK_BUSY;
	    }
	  if( status & CB_STAT_DF)
	    {
	      return ERR_DEVICE_FAULT;
	    }
	  if( status & CB_STAT_DRQ)
	    {
	      return ERR_TRANSMIT_BUFFER_NOT_EMPTY;
	    }
	  if ( status &  CB_STAT_ERR )
	    {
	      return ERR_HARDWARE_ERROR;
	    }
	  // All sectors have been read without error, go to READ_DONE.
	  break;   // go to READ_DONE
	}
      // This is the end of the read loop.  If we get here, the loop is
      // repeated to read the next sector.  Go back to READ_LOOP.
      
    }
  return NO_ERR;
}
static int reg_pio_data_out( int bus,int dev, int cmd,int fr, int sc,
			     unsigned int cyl, int head, int sect,
			     const uint8 *output,unsigned int numSect, int multiCnt )
{
   unsigned char devHead;
   unsigned char devCtrl;
   unsigned char cylLow;
   unsigned char cylHigh;
   unsigned char status;
   unsigned int wordCnt;
   uint16				*buffer = (uint16*)output;

   // mark start of PDI cmd in low level trace
   
   // setup register values and adjust parameters
   devCtrl = CB_DC_HD15 | CB_DC_NIEN;
   devHead = dev ? CB_DH_DEV1 : CB_DH_DEV0;
   devHead = devHead | ( head & 0x4f );
   cylLow = cyl & 0x00ff;
   cylHigh = ( cyl & 0xff00 ) >> 8;
   if ( cmd == CMD_WRITE_BUFFER )
     numSect = 1;
   // only Write Multiple and CFA Write Multiple W/O Erase uses multCnt
   if (    ( cmd != CMD_WRITE_MULTIPLE )
	   && ( cmd != CMD_CFA_WRITE_MULTIPLE_WO_ERASE )
	   )
     multiCnt = 1;
   // Select the drive - call the sub_select function.
   // Quit now if this fails.
   if ( ide_select_device( bus,dev ) )
     {
       return ERR_EXPIRED_TIME_OUT;
     }
   // Set up all the registers except the command register.
   
   pio_outbyte( CB_DC, devCtrl );
   pio_outbyte( CB_FR, fr );
   pio_outbyte( CB_SC, sc );
   pio_outbyte( CB_SN, sect );
   pio_outbyte( CB_CL, cylLow );
   pio_outbyte( CB_CH, cylHigh );
   pio_outbyte( CB_DH, devHead );
   
   // Start the command by setting the Command register.  The drive
   // should immediately set BUSY status.
   pio_outbyte( CB_CMD, cmd );

   // Waste some time by reading the alternate status a few times.
   // This gives the drive time to set BUSY in the status register on
   // really fast systems.  If we don't do this, a slow drive on a fast
   // system may not set BUSY fast enough and we would think it had
   // completed the command when it really had not even started the
   // command yet.
   
   DELAY400NS;
   ide_wait_busy();
   status = pio_inbyte( CB_STAT );
   while ( 1 )
     {
       if ( ( status & ( CB_STAT_BSY | CB_STAT_DRQ ) ) == CB_STAT_DRQ )
	 {
	   wordCnt = multiCnt ? multiCnt : 1;
	   if ( wordCnt > numSect )
	     wordCnt = numSect;
	   wordCnt = wordCnt * 256;
	   pio_rep_outword( CB_DATA, buffer, wordCnt );
	   DELAY400NS;
	   numSect = numSect - ( multiCnt ? multiCnt : 1 );
	   buffer+=wordCnt;
	 }
       
       // So was there any error condition?
         if( status & CB_STAT_BSY)
	{
	  return ERR_DISK_BUSY;
	}
      if( status & CB_STAT_DF)
	{
	  return ERR_DEVICE_FAULT;
	}
      if(status & CB_STAT_ERR)
	{
	  return ERR_HARDWARE_ERROR;
	}
      if ( ( status & CB_STAT_DRQ ) == 0 )
	{
	  return ERR_DRQ_NOT_SET;
	}
       ide_delay( bus,dev );
       ide_reg_poll();
       if ( numSect < 1 )
	 {
	   if ( status & ( CB_STAT_BSY | CB_STAT_DF | CB_STAT_ERR ) )
	     {
	       dprintf("status = 0x%x\n", status);
	       return 1;
	     }
	   break;
	 }
     }
   return 0;
}
static void ide_btochs(uint32 block, ide_device *dev, int *cylinder, int *head, int *sect)
{
  *sect = (block % dev->hardware_device.sectors) + 1;
  block /= dev->hardware_device.sectors;
  *head = (block % dev->hardware_device.heads) | (dev->device ? DH_DRV1 : 0);
  block /= dev->hardware_device.heads;
  *cylinder = block & 0xFFFF;
//  dprintf("ide_btochs: block %d -> cyl %d head %d sect %d\n", block, *cylinder, *head, *sect);
}
static void ide_btolba(uint32 block, ide_device *dev, int *cylinder,int *head, int *sect)
{
  *sect = block & 0xFF;
  *cylinder = (block >> 8) & 0xFFFF;
  *head = ((block >> 24) & 0xF) | ( dev->device ? DH_DRV1: 0) | DH_LBA;
//  dprintf("ide_btolba: block %d -> cyl %d head %d sect %d\n", block, *cylinder, *head, *sect);
}

int	ide_read_block(ide_device	*device,
		       char		*data,
		       uint32		block,
		       uint8		numSectors)
{
  int cyl, head, sect;
  if(device->lba_supported)
    ide_btolba(block, device, &cyl, &head, &sect);
  else
    ide_btochs(block, device, &cyl, &head, &sect);
    
  return reg_pio_data_in( device->bus,device->device, 
			  CMD_READ_SECTORS,
			  0, numSectors,
			  cyl, head, sect,
			  data,
			  numSectors, 2 );
}

int	ide_write_block(ide_device	*device,
			const char	*data,
			uint32		block,
		        uint8		numSectors)
{
  int cyl, head, sect;
  if(device->lba_supported)
    ide_btolba(block, device, &cyl, &head, &sect);
  else
    ide_btochs(block, device, &cyl, &head, &sect);
  return reg_pio_data_out( device->bus,device->device, 
			   CMD_WRITE_SECTORS,
			   0, numSectors,
			   cyl, head, sect,
			   data,
			   numSectors, 2 );
}

void ide_string_conv (char *str, int len)
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

int ide_reset (int bus, int device)
{
  unsigned char	status;
  unsigned char devCtrl;
  devCtrl = CB_DC_HD15 | ( CB_DC_NIEN );
	
  pio_outbyte( CB_DC, devCtrl | CB_DC_SRST );
  DELAY400NS;
  pio_outbyte( CB_DC, devCtrl );
  DELAY400NS;
  if(ide_wait_busy()==1)
    return 1;
  pio_outbyte( CB_DH, device ? CB_DH_DEV1 : CB_DH_DEV0 );
  DELAY400NS;
  return 0;
}
int		ide_drive_present(int bus,int device)
{
  uint8		sc,sn,ch,cl,st;
  unsigned char devCtrl = CB_DC_HD15 | CB_DC_NIEN ;
  int		ret = NO_DEVICE;
  
  pio_outbyte(CB_DC,devCtrl);
  if(device==0)
    pio_outbyte(CB_DH,CB_DH_DEV0);
  else
    pio_outbyte(CB_DH,CB_DH_DEV1);
  DELAY400NS;
  pio_outbyte( CB_SC, 0x55 );
  pio_outbyte( CB_SN, 0xaa );
  pio_outbyte( CB_SC, 0xaa );
  pio_outbyte( CB_SN, 0x55 );
  pio_outbyte( CB_SC, 0x55 );
  pio_outbyte( CB_SN, 0xaa );
  sc = pio_inbyte( CB_SC );
  sn = pio_inbyte( CB_SN );
  pio_outbyte( CB_DH, CB_DH_DEV0 );
  DELAY400NS;
  ide_reset( bus, device );
  
  if(device==0)
    pio_outbyte( CB_DH, CB_DH_DEV0 );
  else
    pio_outbyte( CB_DH, CB_DH_DEV1 );
  DELAY400NS;
  DELAY400NS;
  sc = pio_inbyte( CB_SC );
  sn = pio_inbyte( CB_SN );
  if ( ( sc == 0x01 ) && ( sn == 0x01 ) )
    {
      ret = UNKNOWN_DEVICE;
      cl = pio_inbyte( CB_CL );
      ch = pio_inbyte( CB_CH );
      st = pio_inbyte( CB_STAT );
      if ( ( cl == 0x14 ) && ( ch == 0xeb ) )
	ret = ATAPI_DEVICE;
      else
	if ( ( cl == 0x00 ) && ( ch == 0x00 ) && ( st != 0x00 ) )
	  ret = ATA_DEVICE;
    }
  if(ret!=NO_DEVICE)
    {
      if(device==0)
	pio_outbyte( CB_DH, CB_DH_DEV0 );
      else
	pio_outbyte( CB_DH, CB_DH_DEV1 );
      DELAY400NS;
    }
  return ret;

}
int		ide_identify_device (int bus, int device)
{
  int				ret;
  uint8			*buffer;
  ret = ide_drive_present(bus,device);
  devices[(bus*2) + device].device_type = ret;
  devices[(bus*2) + device].bus = bus;
  devices[(bus*2) + device].device = device;
	
  if(ret>0)
    {
      buffer = (uint8*)&devices[(bus*2) + device].hardware_device;
      ret = reg_pio_data_in(bus,device,
			    CMD_IDENTIFY_DEVICE,
			    1, 0,
			    0, 0, 0,
			    buffer,
			    1, 0
			    );
      ide_string_conv(devices[(bus*2) + device].hardware_device.model,40);      		
      devices[(bus*2) + device].sector_count = 
	devices[(bus*2) + device].hardware_device.cyls*
	devices[(bus*2) + device].hardware_device.heads*
	devices[(bus*2) + device].hardware_device.sectors;
      devices[(bus*2) + device].bytes_per_sector = 512;
      devices[(bus*2) + device].lba_supported = devices[(bus*2) + device].hardware_device.capabilities & DRIVE_SUPPORT_LBA;
      devices[(bus*2) + device].start_block = 0;
      devices[(bus*2) + device].end_block = devices[(bus*2) + device].sector_count +  devices[(bus*2) + device].start_block;
      
      dprintf ("ide: disk at bus %d, device %d %s\n", bus, device,devices[(bus*2) + device].hardware_device.model);
      dprintf ("ide/%d/%d: %dMB; %d cyl, %d head, %d sec, %d bytes/sec  (LBA=%d)\n",bus, device, 
	       devices[(bus*2) + device].sector_count * devices[(bus*2) + device].bytes_per_sector / 1024,
	       devices[(bus*2) + device].hardware_device.cyls,
	       devices[(bus*2) + device].hardware_device.heads,
	       devices[(bus*2) + device].hardware_device.sectors,
	       devices[(bus*2) + device].bytes_per_sector, 
	       devices[(bus*2) + device].lba_supported
	       );
      return 0;
    }
  return -1;
}

int	ide_raw_init(int base1,int base2)
{
 	 pio_base_addr1 = base1;
   pio_base_addr2 = base2;
   pio_memory_seg = 0;
   pio_reg_addrs[ CB_DATA ] = pio_base_addr1 + 0;  // 0
   pio_reg_addrs[ CB_FR   ] = pio_base_addr1 + 1;  // 1
   pio_reg_addrs[ CB_SC   ] = pio_base_addr1 + 2;  // 2
   pio_reg_addrs[ CB_SN   ] = pio_base_addr1 + 3;  // 3
   pio_reg_addrs[ CB_CL   ] = pio_base_addr1 + 4;  // 4
   pio_reg_addrs[ CB_CH   ] = pio_base_addr1 + 5;  // 5
   pio_reg_addrs[ CB_DH   ] = pio_base_addr1 + 6;  // 6
   pio_reg_addrs[ CB_CMD  ] = pio_base_addr1 + 7;  // 7
   pio_reg_addrs[ CB_DC   ] = pio_base_addr2 + 6;  // 8
   pio_reg_addrs[ CB_DA   ] = pio_base_addr2 + 7;  // 9
   return 0;
}

int	ide_get_partition_info(ide_device *device,tPartition *partition,uint32 position)
{
  char		buffer[512];
  uint16	*partitionBuffer = (uint16*)&buffer;		
  if(ide_read_block(device,(char*)&buffer,position,1)!=0)
    {
      dprintf("unable to read partition table\n");
      return -1;
    }
  if(partitionBuffer[PART_MAGIC_OFFSET/2]!=PARTITION_MAGIC)
    {
      dprintf("partition table magic is incorrect\n");
      return -1;
    }
   memcpy(partition, partitionBuffer + PARTITION_OFFSET / 2, sizeof(tPartition)*PARTITION_TBL_SIZE);
  return 0;
}


int	ide_get_partitions(ide_device *device)
{
  int		i;
  memset((tPartition*)&device->partitions,0,sizeof(tPartition)*2* PARTITION_TBL_SIZE);
  if(ide_get_partition_info(device,(tPartition*)&device->partitions,0)!=0)
    return -1;
  
  dprintf("Primary Partition Table\n");
  for (i = 0; i < PARTITION_TBL_SIZE; i++)
    {
      dprintf("  %d: flags:%x type:%x start:%d:%d:%d end:%d:%d:%d stblk:%d count:%d\n", 
	      i, 
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
  if(device->partitions[1].partition_type == PTDosExtended)
    {
      int	extOffset = device->partitions[1].starting_block;
      if(ide_get_partition_info(device,(tPartition*)&device->partitions[4],extOffset)!=0)
	return -1;
      dprintf("Extended Partition Table\n");
      for (i = 4; i < 4+PARTITION_TBL_SIZE; i++)
	{
	  device->partitions[i].starting_block += extOffset;
	  dprintf("  %d: flags:%x type:%x start:%d:%d:%d end:%d:%d:%d stblk:%d count:%d\n", 
		  i, 
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
  return 0;
}
