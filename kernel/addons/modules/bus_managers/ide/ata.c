/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "ide_internal.h"
#include "ata.h"

#include <kernel/debug.h>

#include "ide_sim.h"
#include "basic_prot.h"
#include "sync.h"
#include "PIO.h"
#include "DMA.h"
#include "ide_cmds.h"
#include "queuing.h"

static bool check_rw_status( ide_device_info *device, ide_qrequest *qrequest, bool drq_required )
{
	ide_bus_info *bus = device->bus;
	int status;
	
	status = bus->controller->get_altstatus( bus->channel );
	
	if( (status & ide_status_bsy) != 0 ) {
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		return false;
	}
	
	if( drq_required != ((status & ide_status_drq) != 0) ) {
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
	
	return true;
}


void ata_dpc_PIO( ide_qrequest *qrequest )
{
	ide_device_info *device = qrequest->device;
	bigtime_t timeout = qrequest->request->cam_timeout > 0 ? 
		qrequest->request->cam_timeout : IDE_STD_TIMEOUT;
	
	if( !check_rw_error( device, qrequest ) ||
		!check_rw_status( device, qrequest,
		qrequest->is_write ? device->left_blocks > 1 : true )) 
	{
		/*if( (device->sense.sense_key == SCSIS_KEY_ABORTED_COMMAND ||
			 device->sense.ascq == SCSIS_ASC_UNREC_READ_ERR) &&
			++qrequest->retries < MAX_PIO_RETRIES ) 
		{
			finish_rw_request( qrequest, ide_finish_rw_retry );
		} else*/
			finish_checksense( qrequest );
			
		return;
	}

	if( qrequest->is_write ) {
		if( device->left_blocks == 0 )
			goto finish;
			
		if( !wait_for_drq( device )) 
			goto finish;
			
		start_waiting_nolock( device->bus, timeout, ide_state_async_waiting );
		
		// having a too short data buffer shouldn't happen here
		// anyway - we are prepared
		if( write_PIO_block( qrequest, 512 ) == ERR_GENERAL )
			goto finish_cancel_timeout;
			
		--device->left_blocks;

	} else {
		if( device->left_blocks > 0 )
			start_waiting_nolock( device->bus, timeout, ide_state_async_waiting );
		
		// see write
		if( read_PIO_block( qrequest, 512 ) == ERR_GENERAL ) 
			goto finish_cancel_timeout;
			
		--device->left_blocks;
		
		if( device->left_blocks == 0 ) {
			wait_for_drqdown( device );
			goto finish_cancel_timeout;
		}
	}
	
	//start_waiting( device->bus );
	return;
	
finish_cancel_timeout:
	cancel_irq_timeout( device->bus );
	
finish:
	finish_checksense( qrequest );
	return;
}

void ata_dpc_DMA( ide_qrequest *qrequest )
{
	ide_device_info *device = qrequest->device;
	//ide_bus_info *bus = device->bus;
	bool dma_err, dev_err;
	
	dma_err = finish_dma( device );
	dev_err = check_rw_error( device, qrequest );
	
	if( !dma_err && !dev_err ) {
		device->DMA_failures = 0;
		device->CQ_failures = 0;
		qrequest->request->cam_resid = 0;
		finish_checksense( qrequest );
	} else {
		if( ++device->DMA_failures == MAX_DMA_FAILURES ) {
			device->DMA_enabled = false;
			finish_reset_queue( qrequest );
		} else {
			finish_retry( qrequest );
		}
	}
}

uint8 cmd_48[2][2] = 
{
	{ IDE_CMD_READ_SECTORS_EXT, IDE_CMD_WRITE_SECTORS_EXT },
	{ IDE_CMD_READ_DMA_EXT, IDE_CMD_WRITE_DMA_EXT }
};

uint8 cmd_28[2][2] = 
{
	{ IDE_CMD_READ_SECTORS, IDE_CMD_WRITE_SECTORS },
	{ IDE_CMD_READ_DMA, IDE_CMD_WRITE_DMA }
};

static bool create_rw_taskfile( ide_device_info *device, ide_qrequest *qrequest, 
	uint64 pos, size_t length, bool write )
{
	if( device->use_LBA ) {
		if( device->use_48bits && 
			(pos + length > 0xfffffff ||
			length > 0x100 ))
		{
			if( length > 0xffff ) 
				goto err;
				
			if( qrequest->queuable ) {
				device->tf_param_mask = 
					ide_mask_features_48 |
					ide_mask_sector_count |
					ide_mask_LBA_low_48 |
					ide_mask_LBA_mid_48	|
					ide_mask_LBA_high_48;
					
				device->tf.queued48.sector_count_0_7 = length & 0xff;
				device->tf.queued48.sector_count_8_15 = (length >> 8) & 0xff;
				device->tf.queued48.tag = qrequest->tag;
				device->tf.queued48.lba_0_7 = pos & 0xff;
				device->tf.queued48.lba_8_15 = (pos >> 8) & 0xff;
				device->tf.queued48.lba_16_23 = (pos >> 16) & 0xff;
				device->tf.queued48.lba_24_31 = (pos >> 24) & 0xff;
				device->tf.queued48.lba_32_39 = (pos >> 32) & 0xff;
				device->tf.queued48.lba_40_47 = (pos >> 40) & 0xff;
				device->tf.queued48.command = write ? IDE_CMD_WRITE_DMA_QUEUED 
					: IDE_CMD_READ_DMA_QUEUED;
				return true;
				
			} else {
				device->tf_param_mask = 
					ide_mask_sector_count_48 |
					ide_mask_LBA_low_48 |
					ide_mask_LBA_mid_48	|
					ide_mask_LBA_high_48;

				device->tf.lba48.sector_count_0_7 = length & 0xff;
				device->tf.lba48.sector_count_8_15 = (length >> 8) & 0xff;
				device->tf.lba48.lba_0_7 = pos & 0xff;
				device->tf.lba48.lba_8_15 = (pos >> 8) & 0xff;
				device->tf.lba48.lba_16_23 = (pos >> 16) & 0xff;
				device->tf.lba48.lba_24_31 = (pos >> 24) & 0xff;
				device->tf.lba48.lba_32_39 = (pos >> 32) & 0xff;
				device->tf.lba48.lba_40_47 = (pos >> 40) & 0xff;
				device->tf.lba48.command = cmd_48[qrequest->uses_dma][write];
				return true;
				
			}
		} else {
			if( length > 0x100 ) 
				goto err;

			if( qrequest->queuable ) {
				device->tf_param_mask = 
					ide_mask_features |
					ide_mask_sector_count |
					ide_mask_LBA_low |
					ide_mask_LBA_mid |
					ide_mask_LBA_high |
					ide_mask_device_head;
					
				device->tf.queued.sector_count = length & 0xff;
				device->tf.queued.tag = qrequest->tag;
				device->tf.queued.lba_0_7 = pos & 0xff;
				device->tf.queued.lba_8_15 = (pos >> 8) & 0xff;
				device->tf.queued.lba_16_23 = (pos >> 16) & 0xff;
				device->tf.queued.lba_24_27 = (pos >> 24) & 0xf;
				device->tf.queued.command = write ? IDE_CMD_WRITE_DMA_QUEUED 
					: IDE_CMD_READ_DMA_QUEUED;
				return true;
				
			} else {
				device->tf_param_mask = 
					ide_mask_sector_count |
					ide_mask_LBA_low |
					ide_mask_LBA_mid |
					ide_mask_LBA_high |
					ide_mask_device_head;

				device->tf.lba.sector_count = length & 0xff;
				device->tf.lba.lba_0_7 = pos & 0xff;
				device->tf.lba.lba_8_15 = (pos >> 8) & 0xff;
				device->tf.lba.lba_16_23 = (pos >> 16) & 0xff;
				device->tf.lba.lba_24_27 = (pos >> 24) & 0xf;
				device->tf.lba.command = cmd_28[qrequest->uses_dma][write];
				return true;
				
			}
		}
	} else {
		uint32 track_size, cylinder_offset, cylinder;
		ide_device_infoblock *infoblock = &device->infoblock;
		
		if( length > 0x100 ) 
			goto err;
		
		device->tf.chs.mode = ide_mode_chs;
		
		device->tf_param_mask = 
			ide_mask_sector_count |
			ide_mask_sector_number |
			ide_mask_cylinder_low |
			ide_mask_cylinder_high |
			ide_mask_device_head;
			
		device->tf.chs.sector_count = length & 0xff;
	
		track_size = infoblock->current_heads * infoblock->current_sectors;
		
		if( track_size == 0 ) {
			set_sense( device, 
				SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_MEDIUM_FORMAT_CORRUPTED );
			return false;
		}
		
		cylinder = pos / track_size;

		device->tf.chs.cylinder_0_7 = cylinder & 0xff;
		device->tf.chs.cylinder_8_15 = (cylinder >> 8) & 0xff;

		cylinder_offset = pos - cylinder * track_size;

		device->tf.chs.sector_number = (cylinder_offset % infoblock->current_sectors + 1) & 0xff;
		device->tf.chs.head = cylinder_offset / infoblock->current_sectors;
		
		device->tf.chs.command = cmd_28[qrequest->uses_dma][write];
		return true;
	}

	return true;
	
err:	
	set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_CDB_FIELD );
	return false;
}

void ata_send_rw( ide_device_info *device, ide_qrequest *qrequest,
	uint64 pos, size_t length, bool write )
{
	ide_bus_info *bus = device->bus;
	bigtime_t timeout;
	
	qrequest->is_write = write;
	qrequest->uses_dma = device->DMA_enabled;
	
	if( qrequest->uses_dma ) {
		if( !prepare_dma( device, qrequest )) {
			// if command queueing is used and there is another command
			// already running, PIO commands cannot be used -> declare
			// command as not queuable and resubmit it
			// (XXX this is not fine if the caller wants to recycle the CCB)
			if( device->num_running_reqs > 1 ) {
				qrequest->request->cam_tag_action = 0;
				finish_retry( qrequest );
				return;
			}
			
			qrequest->uses_dma = false;
		}
	}
	
	if( !qrequest->uses_dma ) {
		prep_PIO_transfer( device, qrequest );
		device->left_blocks = length >> 9;
	}

	if( !create_rw_taskfile( device, qrequest, pos, length, write ))
		goto err_setup;
	
	timeout = qrequest->request->cam_timeout > 0 ? 
		qrequest->request->cam_timeout : IDE_STD_TIMEOUT;
	
	if( !send_command( device, !device->is_atapi, 
		timeout,
		!qrequest->uses_dma && !qrequest->is_write ? 
			ide_state_async_waiting : ide_state_accessing )) 
		goto err_send;
	
	if( qrequest->uses_dma ) {	
		if( device->CQ_enabled ) {
			qrequest->running = true;
			
			if( !wait_for_drdy( device ))
				goto err_send;
			
			if( !check_rw_error( device, qrequest ))
				goto err_send;
			
			if( device_released_bus( device )) {
				bus->active_qrequest = NULL;
				
				timer_set_event( IDE_RELEASE_TIMEOUT, 
					TIMER_MODE_ONESHOT, &device->reconnect_timer );	

				device->no_reconnect_timeout = false;
				
				access_finished( bus, device );
				return;
			}
		} 
		
		start_dma( device, qrequest );
		
	} else {
		if( qrequest->is_write ) {
			xpt->schedule_dpc( bus->xpt_cookie, bus->irq_dpc, ide_dpc, bus );
		}
	}
	
	return;
	
err_setup:
	abort_dma( device, qrequest );
	finish_checksense( qrequest );
	return;
	
err_send:
	abort_dma( device, qrequest );
	finish_reset_queue( qrequest );
	return;
}

bool check_rw_error( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	int status;
	
	status = bus->controller->get_altstatus( bus->channel );

	if( (status & ide_status_err) != 0 ) {
		int error;
		
		if( bus->controller->read_command_block_regs( bus->channel, 
			&device->tf, ide_mask_error ) != NO_ERROR )
		{
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
			return false;
		}
		
		error = device->tf.read.error;

		if( (error & ide_error_icrc) != 0 ) {
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_CRC );
			return false;
		}
		
		if( qrequest->is_write ) {
			if( (error & ide_error_wp) != 0 ) {
				set_sense( device, SCSIS_KEY_DATA_PROTECT, SCSIS_ASC_WRITE_PROTECTED );
				return false;
			}
		} else {
			if( (error & ide_error_unc) != 0 ) {
				set_sense( device, SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_UNREC_READ_ERR );
				return false;
			}
		}
		
		if( (error & ide_error_mc) != 0 ) {
			set_sense( device, SCSIS_KEY_UNIT_ATTENTION, SCSIS_ASC_MEDIUM_CHANGED );
			return false;
		}
		
		if( (error & ide_error_idnf) != 0 ) {
			// ID not found - invalid CHS mapping (was: seek error?)
			set_sense( device, SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_RANDOM_POS_ERROR );
			return false;
		}
		
		if( (error & ide_error_mcr) != 0 ) {
			// XXX proper sense key?
			// for TUR this case is not defined !?
			set_sense( device, SCSIS_KEY_UNIT_ATTENTION, SCSIS_ASC_REMOVAL_REQUESTED );
			return false;
		}
		
		if( (error & ide_error_nm) != 0 ) {
			set_sense( device, SCSIS_KEY_NOT_READY, SCSIS_ASC_NO_MEDIUM );
			return false;
		}
		
		if( (error & ide_error_abrt) != 0 ) {
			set_sense( device, SCSIS_KEY_ABORTED_COMMAND, SCSIS_ASC_NO_SENSE );
			return false;
		}

		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
	
	return true;
}

bool check_output( ide_device_info *device, bool drdy_required,
	int error_mask, bool is_write )
{
	ide_bus_info *bus = device->bus;
	int status;
	
	if( bus->sync_wait_timeout ) {
		bus->sync_wait_timeout = false;
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		return false;
	}
	
	status = bus->controller->get_altstatus( bus->channel );
	
	if( (status & ide_status_bsy) != 0 ) {
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		return false;
	}
	
	if( drdy_required && ((status & ide_status_drdy) == 0) ) {
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}

	if( (status & ide_status_err) != 0 ) {
		int error;
		
		if( bus->controller->read_command_block_regs( bus->channel, 
			&device->tf, ide_mask_error ) != NO_ERROR )
		{
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
			return false;
		}
		
		error = device->tf.read.error & error_mask;
		
		if( (error & ide_error_icrc) != 0 ) {
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_CRC );
			return false;
		}
		
		if( is_write ) {
			if( (error & ide_error_wp) != 0 ) {
				set_sense( device, SCSIS_KEY_DATA_PROTECT, SCSIS_ASC_WRITE_PROTECTED );
				return false;
			}
		} else {
			if( (error & ide_error_unc) != 0 ) {
				set_sense( device, SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_UNREC_READ_ERR );
				return false;
			}
		}
		
		if( (error & ide_error_mc) != 0 ) {
			// XXX what are the additional sense data for "medium changed" ?
			set_sense( device, SCSIS_KEY_UNIT_ATTENTION, SCSIS_ASC_NO_SENSE );
			return false;
		}
		
		if( (error & ide_error_idnf) != 0 ) {
			// XXX strange error code, don't really know what it means
			set_sense( device, SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_RANDOM_POS_ERROR );
			return false;
		}
		
		if( (error & ide_error_mcr) != 0 ) {
			// XXX proper sense key?
			set_sense( device, SCSIS_KEY_UNIT_ATTENTION, SCSIS_ASC_REMOVAL_REQUESTED );
			return false;
		}
		
		if( (error & ide_error_nm) != 0 ) {
			set_sense( device, SCSIS_KEY_MEDIUM_ERROR, SCSIS_ASC_NO_MEDIUM );
			return false;
		}
		
		if( (error & ide_error_abrt) != 0 ) {
			set_sense( device, SCSIS_KEY_ABORTED_COMMAND, SCSIS_ASC_NO_SENSE );
			return false;
		}

		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
	
	return true;
}

// set subcommand before calling
static bool device_set_feature( ide_device_info *device, int feature )
{
	device->tf_param_mask = ide_mask_features;
	
	device->tf.write.features = feature;
	device->tf.write.command = IDE_CMD_SET_FEATURES;
	
	if( !send_command( device, true, 1000000, ide_state_sync_waiting ))
		return false;

	sem_acquire( device->bus->sync_wait_sem, 1 );
	
	return check_output( device, true, ide_error_abrt, false );
}

static bool configure_rmsn( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	int i;
	
	if( !device->infoblock.RMSN_supported ||
		device->infoblock._127_RMSN_support != 1 )
		return true;

	if( !device_set_feature( device, IDE_CMD_SET_FEATURES_ENABLE_MSN ))
		return false;

	bus->controller->read_command_block_regs( bus->channel, &device->tf, 
		ide_mask_LBA_mid | ide_mask_LBA_high );

	for( i = 0; i < 5; ++i ) {
		// don't use TUR as it checks not ide_error_mcr | ide_error_mc | ide_error_wp
		// but: we don't check wp as well
		device->combined_sense = 0;
		
		device->tf_param_mask = 0;
		device->tf.write.command = IDE_CMD_GET_MEDIA_STATUS;
		
		if( !send_command( device, true, 15000000, ide_state_sync_waiting )) 
			continue;
	
		if( check_output( device, true, 
			ide_error_nm | ide_error_abrt | ide_error_mcr | ide_error_mc,
			true )
			|| decode_sense_asc_ascq( device->combined_sense ) == SCSIS_ASC_NO_MEDIUM )
			return true;
	}

	return false;
}

static bool configure_CQ( ide_device_info *device )
{
	device->CQ_enabled = device->CQ_supported = false;
	
	if( !device->bus->controller_params.can_CQ ||
		!device->infoblock.DMA_QUEUED_supported ) 
		return initialize_qreq_array( device, 1 );
		
	// seems like the following functions always fail
	if( !device_set_feature( device, IDE_CMD_SET_FEATURES_DISABLE_REL_INT )) {
		dprintf( "Cannot disable release int\n" );
	}
		
	if( !device_set_feature( device, IDE_CMD_SET_FEATURES_DISABLE_SERV_INT )) {
		dprintf( "Cannot disable release int\n" );
	}
	
	device->CQ_enabled = device->CQ_supported = true;
	
	// official IBM docs talk about 31 queue entries, though 
	// their disks report 32; let's hope their docs are wrong
	return initialize_qreq_array( device, device->infoblock.queue_depth + 1 );
}

bool prep_ata( ide_device_info *device )
{
	ide_device_infoblock *infoblock = &device->infoblock;
	uint32 chs_capacity;
	
	SHOW_FLOW0( 3, "" );
	
	device->is_atapi = false;
	device->exec_io = ata_exec_io;
	
	// warning: ata == 0 means "this is ata"...
	if( infoblock->_0.ata.ATA != 0 ) {
		// CF has either magic header or CFA bit set
		// we merge it to "CFA bit set" for easier (later) testing
		if( *(uint16 *)infoblock == 0x848a )
			infoblock->CFA_supported = true;
		else
			return false;
	}
	
	SHOW_FLOW0( 3, "1" );

	if( !infoblock->_54_58_valid ) {
		// normally, current_xxx contains active CHS mapping,
		// but if BIOS didn't call INITIALIZE DEVICE PARAMETERS
		// the default mapping is used
		infoblock->current_sectors = infoblock->sectors;
		infoblock->current_cylinders = infoblock->cylinders;
		infoblock->current_heads = infoblock->heads;
	} 
	
	// just in case capacity_xxx isn't initialized - calculate it manually
	// (seems that this information is really redundant; hopefully)
	chs_capacity = infoblock->current_sectors * infoblock->current_cylinders * 
		infoblock->current_heads;
		
	infoblock->capacity_low = chs_capacity & 0xff;
	infoblock->capacity_high = chs_capacity >> 8;
	
	// checking LBA_supported flag should be sufficient, but it seems
	// that checking LBA_total_sectors is a good idea
	device->use_LBA = infoblock->LBA_supported && infoblock->LBA_total_sectors != 0;
	
	if( device->use_LBA ) {
		device->total_sectors = infoblock->LBA_total_sectors;
		device->tf.lba.mode = ide_mode_lba;
	} else {
		device->total_sectors = chs_capacity;
		device->tf.chs.mode = ide_mode_chs;
	}
		
	device->use_48bits = infoblock->_48_bit_addresses_supported;
	
	if( device->use_48bits )
		device->total_sectors = infoblock->LBA48_total_sectors;
		
	SHOW_FLOW0( 3, "2" );

	if( !configure_DMA( device ) ||
		!configure_CQ( device ) ||
		!configure_rmsn( device ))
		return false;
		
	SHOW_FLOW0( 3, "3" );
		
	return true;
}


void enable_CQ( ide_device_info *device, bool enable )
{
}
