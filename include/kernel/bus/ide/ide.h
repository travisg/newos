/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#include <kernel/bus/scsi/lendian_bitfield.h>
#include <kernel/vfs.h>

typedef union {
	struct {
		uint8	features;
		uint8	sector_count;
		uint8	sector_number;
		uint8	cylinder_0_7;
		uint8	cylinder_8_15;
		LBITFIELD8_3(
			head				: 4,
			device				: 1,
			mode				: 3
		);
		uint8	command;
	} chs;
	struct {
		uint8	features;
		uint8	sector_count;
		uint8	lba_0_7;
		uint8	lba_8_15;
		uint8	lba_16_23;
		LBITFIELD8_3(
			lba_24_27			: 4,
			device				: 1,
			mode				: 3
		);
		uint8	command;
	} lba;
	struct {
		LBITFIELD8_3(
			dma					: 1,
			ovl					: 1,
			_0_res2				: 6
		);
		LBITFIELD8_2(
			_1_res0				: 3,
			tag					: 5
		);
		uint8	_2_res;
		uint8	byte_count_0_7;
		uint8	byte_count_8_15;
		LBITFIELD8_5(
			_5_res0				: 4,
			device				: 1,
			_5_obs5				: 1,
			_5_res6				: 1,
			_5_obs7				: 1
		);
		uint8	command;
	} packet;
	struct {
		LBITFIELD8_5(
			ili					: 1,
			eom					: 1,
			abrt				: 1,
			_0_res3				: 1,
			sense				: 4
		);
		LBITFIELD8_4(
			cmd_or_data			: 1,	// 1 - cmd, 0 - data
			input_or_output	 	: 1,	// 0 - input (to device), 1 - output
			release				: 1,
			tag					: 5
		);
		uint8	_2_res;
		uint8	byte_count_0_7;
		uint8	byte_count_8_15;
		LBITFIELD8_5(
			_4_res0				: 4,
			device				: 1,
			_4_obs5				: 1,
			_4_res6				: 1,
			_4_obs7				: 1
		);
		LBITFIELD8_7(
			chk					: 1,
			_7_res1 				: 2,
			drq					: 1,
			serv				: 1,
			dmrd				: 1,
			drdy				: 1,
			bsy					: 1
		);
	} packet_res;
	struct {
		uint8	sector_count;
		LBITFIELD8_4(
			cmd_or_data			: 1,	// 1 - cmd, 0 - data
			input_or_output		: 1,	// 0 - input (to device), 1 - output
			release				: 1,
			tag					: 5
		);
		uint8	lba_0_7;
		uint8	lba_8_15;
		uint8	lba_16_23;
		LBITFIELD8_3(
			lba_24_27			: 4,
			device				: 1,
			mode				: 3
		);
		uint8	command;
	} queued;
	struct {
		// low order bytes
		uint8	features;
		uint8	sector_count_0_7;
		uint8	lba_0_7;
		uint8	lba_8_15;
		uint8	lba_16_23;
		LBITFIELD8_3(
			_5low_res0			: 4,
			device				: 1,
			mode				: 3
		);
		uint8	command;
		
		// high order bytes
		uint8	_0high_res;
		uint8	sector_count_8_15;
		uint8	lba_24_31;
		uint8	lba_32_39;
		uint8	lba_40_47;
	} lba48;
	struct {
		// low order bytes
		uint8	sector_count_0_7;
		LBITFIELD8_4(
			cmd_or_data			: 1,	// 1 - cmd, 0 - data
			input_or_output	 	: 1,	// 0 - input (to device), 1 - output
			release				: 1,
			tag					: 5
		);
		uint8	lba_0_7;
		uint8	lba_8_15;
		uint8	lba_16_23;
		LBITFIELD8_3(
			_5low_res0				: 4,
			device					: 1,
			mode					: 3
		);
		uint8	command;
		
		// high order bytes
		uint8	sector_count_8_15;
		uint8	_1high_res;
		uint8	lba_24_31;
		uint8	lba_32_39;
		uint8	lba_40_47;
	} queued48;
	struct {
		uint8	_0_res[3];
		uint8	ver;			// RMSN version
		LBITFIELD8_3(
			pena	: 1,		// previously enabled
			lock	: 1,		// capable of locking
			pej		: 1			// can physically eject
		);
	} set_MSN_res;
	struct {
		uint8	r[7+5];
	} raw;
	struct {
		uint8	features;
		uint8	sector_count;
		uint8	sector_number;
		uint8	cylinder_low;
		uint8	cylinder_high;
		uint8	device_head;
		uint8	command;
	} write;
	struct {
		uint8	error;
		uint8	sector_count;
		uint8	sector_number;
		uint8	cylinder_low;
		uint8	cylinder_high;
		uint8	device_head;
		uint8	status;
	} read;
} ide_task_file;

// content of "mode" field
enum {
	ide_mode_chs = 5,
	ide_mode_lba = 7
};

// mask for valid ide_task_file fields
typedef enum {
	ide_mask_features	 		= 0x01,
	ide_mask_sector_count		= 0x02,

	// CHS
	ide_mask_sector_number		= 0x04,
	ide_mask_cylinder_low		= 0x08,
	ide_mask_cylinder_high		= 0x10,
	
	// LBA
	ide_mask_LBA_low			= 0x04,
	ide_mask_LBA_mid			= 0x08,
	ide_mask_LBA_high			= 0x10,

	// packet
	ide_mask_byte_count			= 0x18,
	
	// packet and dma queued result
	ide_mask_error				= 0x01,
	ide_mask_ireason			= 0x02,

	ide_mask_device_head		= 0x20,
	ide_mask_command			= 0x40,
	
	ide_mask_status				= 0x40,

	// for 48 bits, the follow flags declare which registers to load twice
	ide_mask_features_48		= 0x80 | ide_mask_features,
	ide_mask_sector_count_48	= 0x80 | ide_mask_sector_count,
	ide_mask_LBA_low_48			= 0x100 | ide_mask_LBA_low,
	ide_mask_LBA_mid_48			= 0x200 | ide_mask_LBA_mid,
	ide_mask_LBA_high_48		= 0x400 | ide_mask_LBA_high,
	
	ide_mask_HOB				= 0x780

	//ide_mask_all			= 0x7f
} ide_reg_mask;

// status register
enum {
	ide_status_err		= 0x01,		// error
	ide_status_index	= 0x02,		// obsolete
	ide_status_corr		= 0x04,		// obsolete
	ide_status_drq		= 0x08,		// data request
	ide_status_dsc		= 0x10,		// reserved
	ide_status_service	= 0x10,		// ready to service device
	ide_status_dwf		= 0x20,		// reserved
	ide_status_dma		= 0x20,		// reserved
	ide_status_dmrd		= 0x20,		// packet: DMA ready
	ide_status_df		= 0x20,		// packet: disk failure
	ide_status_drdy		= 0x40,		// device ready
	ide_status_bsy		= 0x80		// busy
} ide_status_mask;

// device control register
enum {
									// bit 0 must be zero
	ide_devctrl_nien	= 0x02,		// disable INTRQ
	ide_devctrl_srst	= 0x04,		// software device reset
	ide_devctrl_bit3	= 0x08,		// don't know, but must be set
									// bits inbetween are reserved
	ide_devctrl_hob		= 0x80		// read high order byte (for 48-bit lba)
} ide_devcntrl_mask;

// error register - most bits are command specific
enum {
	// always used
	ide_error_abrt		= 0x04,		// command aborted
	
	// used for Ultra DMA modes
	ide_error_icrc		= 0x80,		// interface CRC error

	// used by reading data transfers
	ide_error_unc		= 0x40,		// uncorrectable data error
	// used by writing data transfers
	ide_error_wp		= 0x40,		// media write protect

	// used by all data transfer commands
	ide_error_mc		= 0x20,		// medium changed
	ide_error_idnf		= 0x10,		// CHS translation not init./ invalid CHS address
	ide_error_mcr		= 0x08,		// media change requested
	ide_error_nm		= 0x02,		// no media (for removable media devices)
} ide_error_mask;

typedef struct ide_controller_params {
	int dma_alignment;				// this must 2^i-1 for some i
	size_t dma_boundary;
	bool dma_boundary_solid;
	int max_sg_num;
	int max_blocks;

	int max_devices;
	int can_DMA;
	int can_CQ;
} ide_controller_params;


typedef struct ide_channel_info *ide_channel_cookie;
	
typedef struct {
	int (*write_command_block_regs)
		(ide_channel_cookie channel, ide_task_file *tf, ide_reg_mask mask);
	int (*read_command_block_regs)
		(ide_channel_cookie channel, ide_task_file *tf, ide_reg_mask mask);

	uint8 (*get_altstatus) (ide_channel_cookie channel);
	int (*write_device_control) (ide_channel_cookie channel, uint8 val);	

	int (*write_pio_16) (ide_channel_cookie channel, uint16 *data, int count, bool force_16bit );
	int (*read_pio_16) (ide_channel_cookie channel, uint16 *data, int count, bool force_16bit );

	int (*prepare_dma)(ide_channel_cookie channel, const iovec *vec, size_t count,
	                        uint32 startbyte, uint32 blocksize,
	                        size_t *numBytes, bool write);
	int (*start_dma)(ide_channel_cookie channel);
	int (*finish_dma)(ide_channel_cookie channel);
	
	// currently, we ignore returned error code
	int (*release)( ide_channel_cookie channel );
} ide_controller_interface;


#define IDE_BUS_MANAGER_NAME "bus_managers/ide/v1"

typedef struct ide_bus_info *ide_bus_cookie;

typedef struct {
	int (*irq_handler)( ide_bus_cookie channel );
	
	// as soon as *bus is written the channel gets used for device 
	// detection, so make sure it's really ready
	int (*connect_channel)( const char *name, 
		ide_controller_interface *controller,
		ide_channel_cookie channel,
		ide_controller_params *params, ide_bus_cookie *bus );
		
	int (*disconnect_channel)( ide_bus_cookie bus );
} ide_bus_manager_interface;


#endif
