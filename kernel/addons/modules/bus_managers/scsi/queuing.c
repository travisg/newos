/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "queuing.h"

void xpt_add_req_queue_first( CCB_SCSIIO *request, xpt_device_info *device )
{
	if( device->queued_reqs ) {
		CCB_SCSIIO *first, *last;
		
		first = device->queued_reqs;
		last = first->xpt_prev;
		
		request->xpt_next = first;
		request->xpt_prev = last;
		first->xpt_prev = request;
		last->xpt_next = request;
	} else {
		request->xpt_next = request->xpt_prev = request;
	}

	device->queued_reqs = request;
	++device->lock_count;
	request->cam_ch.xpt_bus->lock_count += device->in_wait_queue;
}

static void xpt_add_req_queue_last( CCB_SCSIIO *request, xpt_device_info *device )
{
	if( device->queued_reqs ) {
		CCB_SCSIIO *first, *last;
		
		first = device->queued_reqs;
		last = first->xpt_prev;
		
		request->xpt_next = first;
		request->xpt_prev = last;
		first->xpt_prev = request;
		last->xpt_next = request;
	} else {
		request->xpt_next = request->xpt_prev = request;
		device->queued_reqs = request;
	}

	++device->lock_count;	
	request->cam_ch.xpt_bus->lock_count += device->in_wait_queue;
}

void xpt_remove_req_queue( CCB_SCSIIO *request, xpt_device_info *device )
{
	request->xpt_next->xpt_prev = request->xpt_prev;
	request->xpt_prev->xpt_next = request->xpt_next;

	--device->lock_count;	
	request->cam_ch.xpt_bus->lock_count -= device->in_wait_queue;
	
	if( request == device->queued_reqs ) {
		if( request->xpt_next != request )
			device->queued_reqs = request->xpt_next;
		else
			device->queued_reqs = NULL;
	}
}

void xpt_add_device_queue_first( xpt_device_info *device, xpt_bus_info *bus )
{
	if( device->in_wait_queue )
		return;
		
	device->in_wait_queue = true;
	bus->lock_count += device->lock_count - device->sim_overflow - device->blocked_by_sim;
	
	if( bus->waiting_devices ) {
		xpt_device_info *first, *last;
		
		first = bus->waiting_devices;
		last = first->prev_waiting;
		
		device->next_waiting = first;
		device->prev_waiting = last;
		first->prev_waiting = device;
		last->next_waiting = device;
	} else {
		device->next_waiting = device->prev_waiting = device;
	}
	
	bus->waiting_devices = device;
}

void xpt_add_device_queue_last( xpt_device_info *device, xpt_bus_info *bus )
{
	if( device->in_wait_queue )
		return;

	device->in_wait_queue = true;
	bus->lock_count += device->lock_count - device->sim_overflow - device->blocked_by_sim;
	
	if( bus->waiting_devices ) {
		xpt_device_info *first, *last;
		
		first = bus->waiting_devices;
		last = first->prev_waiting;
		
		device->next_waiting = first;
		device->prev_waiting = last;
		first->prev_waiting = device;
		last->next_waiting = device;
	} else {
		device->next_waiting = device->prev_waiting = device;
		bus->waiting_devices = device;
	}
}

void xpt_remove_device_queue( xpt_device_info *device, xpt_bus_info *bus )
{
	if( !device->in_wait_queue )
		return;

	device->in_wait_queue = false;
	bus->lock_count -= device->lock_count - device->sim_overflow - device->blocked_by_sim;
	
	device->next_waiting->prev_waiting = device->prev_waiting;
	device->prev_waiting->next_waiting = device->next_waiting;
	
	if( device == bus->waiting_devices ) {
		if( device->next_waiting != device )
			bus->waiting_devices = device->next_waiting;
		else
			bus->waiting_devices = NULL;
	}
}



static void xpt_insert_new_request( xpt_device_info *device, 
	CCB_SCSIIO *new_request )
{
	CCB_SCSIIO *first, *last, *before;
	
	first = device->queued_reqs;
	
	if( first == NULL ) {
		xpt_add_req_queue_first( new_request, device );
		return;
	}

	// don't let syncs bypass others
	if( new_request->xpt_ordered ) {
		xpt_add_req_queue_last( new_request, device );
		return;
	}

	// we keep all requests ascendingly, bitonically sorted
	// we must let a normal request overtake a sync request
	last = first->xpt_prev;
	
	if( new_request->cam_sort > device->last_sort &&
		new_request->cam_sort > first->cam_sort ) 
	{
		// we should be the first request, make sure we don't bypass syncs
		for( before = last; !before->xpt_ordered; ) {
			before = before->xpt_prev;
			if( before == last )
				break;
		}
		
		if( !before->xpt_ordered )
			xpt_add_req_queue_first( new_request, device );
			return;
	}

	for( before = last; !before->xpt_ordered;  ) {
		if( before->cam_sort < new_request->cam_sort )
			break;
			
		before = before->xpt_prev;
		if( before == last )
			break;
	}

	new_request->xpt_prev = before;
	new_request->xpt_next = before->xpt_next;
	before->xpt_next->xpt_prev = new_request;
	before->xpt_next = new_request;
	
	if( before == first )
		device->queued_reqs = new_request;

	++device->lock_count;
	new_request->cam_ch.xpt_bus->lock_count += device->in_wait_queue;
}

void xpt_add_queued_request( CCB_SCSIIO *request )
{
	xpt_device_info *device = request->cam_ch.xpt_device;
	xpt_bus_info *bus = request->cam_ch.xpt_bus;

	request->cam_ch.xpt_state = XPT_STATE_QUEUED;
	xpt_insert_new_request( device, request );

	xpt_add_device_queue_last( device, bus );
}


