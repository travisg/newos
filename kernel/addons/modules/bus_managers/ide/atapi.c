/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/bus/scsi/scsi_cmds.h>

#include "ide_internal.h"
#include "atapi.h"
#include "ide_cmds.h"
#include "ide_sim.h"
#include "sync.h"
#include "DMA.h"
#include "PIO.h"
#include "basic_prot.h"
#include "queuing.h"
#include "device_mgr.h"


// used for MODE SENSE/SELECT 6 emulation; maximum size is 255 + header,
// so this is a safe bet
#define IDE_ATAPI_BUFFER_SIZE 512


static bool check_packet_error( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	int status;
	
	status = bus->controller->get_altstatus( bus->channel );

	if( (status & (ide_status_err | ide_status_df)) != 0 ) {
		int error;
		
		SHOW_FLOW0( 3, "packet error" );
		
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


void packet_dpc( ide_qrequest *qrequest )
{
	ide_device_info *device = qrequest->device;
	ide_bus_info *bus = device->bus;
	int status;
	bigtime_t timeout = qrequest->request->cam_timeout > 0 ? 
		qrequest->request->cam_timeout * 1000000 : IDE_STD_TIMEOUT;
		
	SHOW_FLOW0( 3, "" );
	
	bus->controller->read_command_block_regs( bus->channel,
		&device->tf, ide_mask_error | ide_mask_ireason );
		
	status = bus->controller->get_altstatus( bus->channel );

	if( qrequest->packet_irq ) {
		qrequest->packet_irq = false;
		if( !device->tf.packet_res.cmd_or_data ||
			device->tf.packet_res.input_or_output ||
			(status & ide_status_drq) == 0 ) 
		{
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_FAILURE );
			goto err;
		}
			
		IDE_LOCK( bus );
			
		if( bus->controller->write_pio_16( bus->channel, 
			(uint16 *)device->packet, sizeof( device->packet ) / sizeof( uint16 ), 
			true ) != NO_ERROR ) 
		{
			IDE_UNLOCK( bus );
			goto err_int;
		}
			
		start_waiting( device->bus, timeout, ide_state_async_waiting );
		return;
	}
	
	SHOW_FLOW0( 3, "2" );
	
	if( qrequest->uses_dma ) {
		bool dma_err, dev_err;

		// don't check drq - if there is some data left, we cannot handle
		// it anyway
		// XXX does the device throw remaining data away on DMA overflow?
		
		dma_err = finish_dma( device );		
		dev_err = check_packet_error( device, qrequest );
		
		adjust_atapi_result( device, qrequest );
		
		if( !dma_err ) {
			device->DMA_failures = 0;
			qrequest->request->cam_resid = 0;
			finish_checksense( qrequest );
		} else {
			if( ++device->DMA_failures == MAX_DMA_FAILURES ) {
				device->DMA_enabled = false;
				finish_checksense( qrequest );
			} else
				finish_retry( qrequest );
		}

		return;
	}
	
	SHOW_FLOW0( 3, "3" );
	
	if( (status & ide_status_drq) != 0 ) {
		int length;
		bool err;
		
		SHOW_FLOW0( 3, "data transmission" );
		
		if( device->tf.packet_res.cmd_or_data ) {
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_FAILURE );
			goto err;
		}
		
		SHOW_FLOW0( 3, "4" );

		if( (device->tf.packet_res.input_or_output ^ qrequest->is_write) == 0 ) {
			// XXX hm, either the device is broken or the caller has specified
			// the wrong direction - what is the proper handling?
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_FAILURE );
			
			//xpt->block_bus( device->bus->xpt_cookie );
			adjust_atapi_result( device, qrequest );
			finish_checksense( qrequest );
			//reset_bus( device );
			//xpt->unblock_bus( device->bus->xpt_cookie );
			return;
		}
		
		SHOW_FLOW0( 3, "5" );

		bus->controller->read_command_block_regs( bus->channel,
			&device->tf, ide_mask_byte_count );
			
		length = device->tf.packet_res.byte_count_0_7 |
			((int)device->tf.packet_res.byte_count_8_15 << 8);
			
		SHOW_FLOW( 3, "device transmittes %i bytes\n", length );
		
		// don't mind getting overtaken by IRQ handler - as he will
		// issue a DPC for the thread context we are in, we are save
		start_waiting_nolock( device->bus, timeout, ide_state_async_waiting );
		
		if( device->tf.packet_res.input_or_output )
			err = read_PIO_block( qrequest, length );
		else
			err = write_PIO_block( qrequest, length );
			
		SHOW_FLOW0( 3, "6" );
			
		// discarding data can happen but is OK
		if( err == ERR_GENERAL )
			goto err_int;
			
		SHOW_FLOW0( 3, "7" );
			
		return;
		
	} else {
		SHOW_FLOW0( 3, "no data" );
		
		check_packet_error( device, qrequest );
		adjust_atapi_result( device, qrequest );
		finish_checksense( qrequest );
		return;
	}
	
	return;
	
err_int:
	set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
	
err:
	adjust_atapi_result( device, qrequest );
	finish_checksense( qrequest );
}

static bool create_packet_taskfile( ide_device_info *device, ide_qrequest *qrequest, 
	bool write )
{
	CCB_SCSIIO *request = qrequest->request;
	int scsi_cmd = request->cam_cdb[0];

	qrequest->uses_dma = device->DMA_enabled && 
		(scsi_cmd == SCSI_OP_READ_6 || scsi_cmd == SCSI_OP_WRITE_6 ||
		 scsi_cmd == SCSI_OP_READ_10 || scsi_cmd == SCSI_OP_WRITE_10 ||
		 scsi_cmd == SCSI_OP_READ_12 || scsi_cmd == SCSI_OP_WRITE_12);
		 
	device->tf_param_mask = ide_mask_features | ide_mask_byte_count;
	
	device->tf.packet.dma = qrequest->uses_dma;
	device->tf.packet.ovl = 0;
	device->tf.packet.byte_count_0_7 = request->cam_dxfer_len & 0xff;
	device->tf.packet.byte_count_8_15 = request->cam_dxfer_len >> 8;
	device->tf.packet.command = IDE_CMD_PACKET;
	
	return true;
}

void send_packet( ide_device_info *device, ide_qrequest *qrequest, bool write )
{
	ide_bus_info *bus = device->bus;
	bool packet_irq = device->atapi.packet_irq;
	
	SHOW_FLOW0( 3, "" );
	
	{
		unsigned int i;
		
		for( i = 0; i < sizeof( device->packet ); ++i ) 
			dprintf( "%x ", device->packet[i] );
	}
		
	qrequest->is_write = write;
	qrequest->packet_irq = packet_irq;
	
	if( qrequest->uses_dma ) {
		if( !prepare_dma( device, qrequest ))
			qrequest->uses_dma = false;
	}
	
	SHOW_FLOW0( 3, "1" );
	
	if( !qrequest->uses_dma )
		prep_PIO_transfer( device, qrequest );
		
	SHOW_FLOW0( 3, "2" );

	if( !create_packet_taskfile( device, qrequest, write ))
		goto err_setup;
		
	SHOW_FLOW0( 3, "3" );
	
	if( !send_command( device, !device->is_atapi, 
		device->atapi.packet_irq_timeout, 
		device->atapi.packet_irq ? ide_state_async_waiting : ide_state_accessing )) 
		goto err_setup;
		
	SHOW_FLOW0( 3, "4" );
	
	if( packet_irq )
		// timeout and stuff is already set up by send_command
		return;
		
	SHOW_FLOW0( 3, "5" );
		
	if( !ide_wait( device, ide_status_drq, ide_status_bsy, false, 100000 )) 
		goto err_setup;
		
	SHOW_FLOW0( 3, "6" );

	// make sure device really asks for command packet		
	bus->controller->read_command_block_regs( bus->channel, &device->tf,
		ide_mask_ireason );
		
	if( !device->tf.packet_res.cmd_or_data ||
		device->tf.packet_res.input_or_output )
	{
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_COM_FAILURE );
		goto err_setup;
	}
	
	SHOW_FLOW0( 3, "7" );
	
	// needed by some slow devices	
	spin( 10 );
	
	if( qrequest->uses_dma ) {
		// don't hurry setting up wait environment - device has to wait for DMA
		if( bus->controller->write_pio_16( bus->channel, 
			(uint16 *)device->packet, sizeof( device->packet ) / sizeof( uint16 ), 
			true ) != NO_ERROR )
		{
			goto err_dma;
		}
			
		start_dma( device, qrequest );
		
	} else {
		bigtime_t timeout = qrequest->request->cam_timeout > 0 ? 
			qrequest->request->cam_timeout * 1000000 : IDE_STD_TIMEOUT;
			
		IDE_LOCK( bus );

		if( bus->controller->write_pio_16( bus->channel, 
			(uint16 *)device->packet, sizeof( device->packet ) / sizeof( uint16 ), 
			true ) != NO_ERROR )
		{
			goto err_pio;
		}

		start_waiting( bus, timeout, ide_state_async_waiting );
	}
	
	SHOW_FLOW0( 3, "8" );
	
	return;
	
err_pio:
	IDE_UNLOCK( bus );
	
err_dma:
	set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
	
err_setup:
	if( qrequest->uses_dma )
		abort_dma( device, qrequest );

	adjust_atapi_result( device, qrequest );		
	finish_checksense( qrequest );
}


bool prep_atapi( ide_device_info *device )
{
	ide_device_infoblock *infoblock = &device->infoblock;
	
	SHOW_FLOW0( 3, "" );

	device->is_atapi = true;
	device->exec_io = atapi_exec_io;
	
	if( infoblock->_0.atapi.ATAPI != 2 )
		return false;
		
	switch( infoblock->_0.atapi.drq_speed ) {
	case 0:
	case 2:
		device->atapi.packet_irq = false;
		break;
	case 1:
		device->atapi.packet_irq = true;
		device->atapi.packet_irq_timeout = IDE_STD_TIMEOUT;
		break;
	default:
		return false;
	}
	
	SHOW_FLOW( 3, "%i", infoblock->_0.atapi.drq_speed );
	
	/*if( infoblock->_0.atapi.packet_size != 0 )
		return false;*/
	
	device->device_type = infoblock->_0.atapi.type;
	
	SHOW_FLOW( 3, "device_type=%i", device->device_type );
	
	if( !initialize_qreq_array( device, 1 ) ||
		!initialize_buffer( device, IDE_ATAPI_BUFFER_SIZE ) ||
		!configure_DMA( device ))
		return false;
		
	return true;
}
