/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/bus/scsi/scsi_cmds.h>
#include "ide_internal.h"
#include "basic_prot.h"

#include "queuing.h"
#include "ide_sim.h"
#include "sync.h"
#include "ide_cmds.h"


// time in µs an IDE interrupt may get delayed
// as this is used for waiting in normal code, this applies to hardware delays only
// it's used for a hardware bug fix as well, see send_command
#define MAX_IRQ_DELAY 50


bool wait_for_drq( ide_device_info *device )
{
	return ide_wait( device, ide_status_drq, 0, true, 10000000 );
}

bool wait_for_drqdown( ide_device_info *device )
{
	return ide_wait( device, 0, ide_status_drq, true, 1000000 );
}

bool wait_for_drdy( ide_device_info *device )
{
	return ide_wait( device, ide_status_drdy, ide_status_bsy, false, 5000000 );
}


// we need a device to store sense information
// (we could just take device 0, but this were not fair if the reset
// was done because of a device 1 failure)
bool reset_bus( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	ide_controller_interface *controller = bus->controller;
	ide_channel_cookie channel = bus->channel;
	
	SHOW_FLOW0( 3, "" );
	
	// activate srst signal for 5 µs
	reset_timeouts( device );
	reset_timeouts( device->other_device );
	//set_irq_state( device, ide_irq_state_ignore );
	
	SHOW_FLOW0( 3, "1" );
	
	if( controller->write_device_control( channel, 
		ide_devctrl_nien | ide_devctrl_srst | ide_devctrl_bit3 ) != NO_ERROR )
		goto err0;
		
	SHOW_FLOW0( 3, "2" );
		
	spin( 5 );
	if( controller->write_device_control( channel, 
		ide_devctrl_nien | ide_devctrl_bit3 ) != NO_ERROR )
		goto err0;
		
	SHOW_FLOW0( 3, "3" );

	// let devices wake up
	thread_snooze( 2000 );
	
	SHOW_FLOW0( 3, "4" );

	// ouch, we have to wait up to 31 seconds!
	if( !ide_wait( device, 0, ide_status_bsy, false, 31000000 )) {
	
		// we don't know which of the devices is broken
		// so we don't disable them
		if( controller->write_device_control( channel, ide_devctrl_bit3 ) != NO_ERROR )
			goto err0;

		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		goto err1;
	}
	
	SHOW_FLOW0( 3, "5" );
	
	if( controller->write_device_control( channel, ide_devctrl_bit3 ) != NO_ERROR )
		goto err0;
		
	SHOW_FLOW0( 3, "6" );
	
	finish_all_requests( bus->devices[0], CAM_SCSI_BUS_RESET );
	finish_all_requests( bus->devices[1], CAM_SCSI_BUS_RESET );
	
	SHOW_FLOW0( 3, "7" );
	
	xpt->call_async( bus->xpt_cookie, -1, -1, AC_BUS_RESET, NULL, 0 );
	
	SHOW_FLOW0( 3, "8" );
	return true;
	
err0:
	set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
	
err1:
	finish_all_requests( bus->devices[0], CAM_SCSI_BUS_RESET );
	finish_all_requests( bus->devices[1], CAM_SCSI_BUS_RESET );
	
	xpt->call_async( bus->xpt_cookie, -1, -1, AC_BUS_RESET, NULL, 0 );
	return false;
}


// new_state must be either accessing, async_waiting or sync_waiting
// param_mask must not include command register
bool send_command( ide_device_info *device, 
	bool need_drdy, bigtime_t timeout, ide_bus_state new_state )
{
	ide_bus_info *bus = device->bus;
	bigtime_t irq_disabled_at = 0; // make compiler happy
	
	bool irq_guard = bus->num_running_reqs > 1;
	
	SHOW_FLOW0( 3, "" );

	reset_timeouts( device );
	
	if( irq_guard ) {
		if( bus->controller->write_device_control( bus->channel, 
			ide_devctrl_nien | ide_devctrl_bit3 ) != NO_ERROR )
			goto err;

		irq_disabled_at = system_time();
	}

	if( bus->controller->write_command_block_regs( bus->channel, &device->tf, 
		ide_mask_device_head ) != NO_ERROR ) 
		goto err;
		
	SHOW_FLOW0( 3, "1" );
		
	bus->active_device = device;
	
	if( !ide_wait( device, 0, ide_status_bsy | ide_status_drq, false, 50000 )) {
		uint8 status;
		
		SHOW_FLOW0( 1, "device is not ready" );
		
		status = bus->controller->get_altstatus( bus->channel );
		if( status == 0xff ) {
			// this shouldn't happen unless the device has died
			// as we only submit commands to existing devices
			// (only detection routines shoot at will)
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
			return false;
		}
		
		if( !reset_bus( device ))
			return false;
			
		SHOW_FLOW0( 1, "retrying" );
		
		if( bus->controller->write_command_block_regs( bus->channel,
			 &device->tf, ide_mask_device_head ) != NO_ERROR )
			goto err;
			
		bus->active_device = device;
		
		if( !ide_wait( device, 0, ide_status_bsy | ide_status_drq, false, 50000 )) {
			// XXX this is not a device but a bus error, we should return
			// CAM_SEL_TIMEOUT instead
			SHOW_FLOW0( 1, "device is dead" );
			set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_LUN_SEL_FAILED );
			return false;
		}
	}
	
	SHOW_FLOW0( 3, "3" );
	
	if( need_drdy &&
		(bus->controller->get_altstatus( bus->channel ) & ide_status_drdy) == 0 ) 
	{
		SHOW_FLOW0( 3, "drdy not set" );
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		return false;
	}
	
	SHOW_FLOW0( 3, "4" );
	
	if( bus->controller->write_command_block_regs( bus->channel, &device->tf, 
		device->tf_param_mask ) != NO_ERROR )
		goto err;
		
	SHOW_FLOW0( 3, "5" );
		
	if( irq_guard ) {
		// enable IRQs now
		// IRQ may be fired by service requests and by the process of disabling(!)
		// them (I heard this is caused by edge triggered PCI IRQs)
		
		// wait at least 50 µs to catch all pending irq's
		// (at my system, up to 30 µs elapsed)
		
		// additionally, old drives (at least my IBM-DTTA-351010) loose
		// sync if they are pushed too hard - on heavy overlapped write 
		// stress this drive tends to forget outstanding requests,
		// waiting at least 50 µs seems(!) to solve this
		while( system_time() - irq_disabled_at < MAX_IRQ_DELAY )
			spin( 1 );

	}
	
	SHOW_FLOW0( 3, "6" );

	if( new_state != ide_state_accessing ) {
		IDE_LOCK( bus );	
	}
	
	SHOW_FLOW( 3, "Writing command %x", (int)device->tf.write.command );
	if( bus->controller->write_command_block_regs( bus->channel, 
		&device->tf, ide_mask_command ) != NO_ERROR ) 
		goto err2;
		
	SHOW_FLOW0( 3, "7" );
		
	if( irq_guard ) {
		if( bus->controller->write_device_control( bus->channel, 
			ide_devctrl_bit3 ) != NO_ERROR ) 
			goto err1;
	}
	
	SHOW_FLOW0( 3, "8" );
		
	if( new_state != ide_state_accessing ) {
		start_waiting( bus, timeout, new_state );
	}
	
	SHOW_FLOW0( 3, "9" );

	return true;

err2:
	if( irq_guard )
		bus->controller->write_device_control( bus->channel, 
			ide_devctrl_bit3 );

err1:
	if( timeout > 0 ) {
		bus->state = ide_state_accessing;
		IDE_UNLOCK( bus );	
	}
	
err:
	set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
	return false;
}

bool ide_wait( ide_device_info *device, int mask, int not_mask, 
	bool check_err, bigtime_t timeout )
{
	ide_bus_info *bus = device->bus;
	bigtime_t start_time = system_time();

	while( 1 ) {
		bigtime_t elapsed_time;
		int status;

		spin( 1 );
		
		status = bus->controller->get_altstatus( bus->channel );
		
		if( (status & mask) == mask &&
			(status & not_mask) == 0 )
			return true;
		
		if( check_err && (status & ide_status_err) != 0 ) {
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
			return false;
		}
		
		elapsed_time = system_time() - start_time;
		
		if( elapsed_time > timeout ) {
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
			return false;
		}
			
		if( elapsed_time > 3000 )
			thread_snooze( elapsed_time / 10 );
	}
}

void spin( bigtime_t duration )
{
	bigtime_t start_time = system_time();
	
	while( system_time() - start_time < duration )
		;
}


bool device_start_service( ide_device_info *device, int *tag )
{
	ide_bus_info *bus = device->bus;
	bigtime_t irq_disabled_at = 0;	// makes compiler happy
	bool irq_guard = bus->num_running_reqs > 1;
	
	device->tf.write.command = IDE_CMD_SERVICE;
	device->tf.queued.mode = ide_mode_lba;

	reset_timeouts( device );
	//set_irq_state( device, ide_irq_state_ignore );
	
	if( irq_guard ) {
		irq_disabled_at = system_time();
		if( bus->controller->write_device_control( bus->channel, 
			ide_devctrl_nien | ide_devctrl_bit3 ) != NO_ERROR )
			goto err;
	}
	
	if( bus->controller->write_command_block_regs( bus->channel, &device->tf, 
		ide_mask_device_head ) != NO_ERROR )
		goto err;
			
	bus->active_device = device;

	if( !ide_wait( device, 0, ide_status_bsy | ide_status_drq, false, 50000 )) {
		// XXX this is not a device but a bus error, we should return
		// CAM_SEL_TIMEOUT instead
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
		
	if( irq_guard ) {
		// see send_ata
		while( system_time() - irq_disabled_at < MAX_IRQ_DELAY )
			spin( 1 );

		if( bus->controller->write_device_control( bus->channel, 
			ide_devctrl_bit3 ) != NO_ERROR )
			goto err;
	}
	
	// here we go...
	if( bus->controller->write_command_block_regs( bus->channel, &device->tf, 
		ide_mask_command ) != NO_ERROR )
		goto err;

	if( !ide_wait( device, ide_status_drdy, ide_status_bsy, false, 1000000 )) {
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );
		return false;
	}
	
	if( bus->controller->read_command_block_regs( bus->channel, &device->tf, 
		ide_mask_sector_count ) != NO_ERROR )
		goto err;
		
	if( device->tf.queued.release ) {
		// bus release is the wrong answer to a service request
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
	
	*tag = device->tf.queued.tag;
	return true;

err:
	set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
	return false;
}

bool check_service_req( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	int status;
	
	if( device->num_running_reqs == 0 )
		return false;

	if( bus->controller->write_command_block_regs( bus->channel, 
		&device->tf, ide_mask_device_head ) != NO_ERROR )
		// on error, pretend that this device asks for service
		// -> the disappeared controller will be recognized soon
		return true;
		
	bus->active_device = device;

	// give one clock (400 ns) to take notice
	spin( 1 );
	
	status = bus->controller->get_altstatus( bus->channel );
	
	return (status & ide_status_service) != 0;
}

