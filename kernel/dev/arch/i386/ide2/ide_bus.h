#ifndef __IDE_BUS_H__
#define	__IDE_BUS_H__

#include <newos/types.h>

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


typedef		struct
{
	char		*name;
	int	(*init)(int channel);
	int	(*read_block)(void *cookie,int drive,long block,void *buffer,size_t size);
	int	(*write_block)(void *cookie,int drive,long block,void *buffer,size_t size);
	int	(*setup_dma)(void *cookie,int dir, long size, uint8 *buffer );
	int	(*start_dma)(void *cookie);
	int	(*finish_dma)(void *cookie);
	int	(*write_register16)(void *cookie,int reg,uint16 value);
	int	(*write_register)(void *cookie,int reg,uint8 value);
	uint16	(*read_register16)(void *cookie,int reg);
	uint8	(*read_register)(void *cookie,int reg);
	uint8	(*get_alt_status)(void *cookie);
	int	(*transfer_buffer)(void *cookie,int reg,void *buffer,size_t size,bool fromMemory);
	int	(*select_drive)(void *cookie,int drive);
	int	(*reset_bus)(void *cookie,int default_drive);
	int	(*delay_on_bus)(void *cookie);
	int	(*wait_busy)(void *cookie);
	void	*(*get_nth_cookie)(int bus);
	void 	*(*get_attached_drive)(void *cookie,int drive);
	void 	*(*get_attached_drive_cookie)(void *cookie,int drive);
	bool	(*support_dma)(void *cookie);
	int	(*lock)(void *cookie);
	int	(*unlock)(void *cookie);
}ide_bus;

extern	void	*bus_cookies[2];
extern	ide_bus	*buses[2];
#endif
