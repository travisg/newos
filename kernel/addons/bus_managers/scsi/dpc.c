/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "dpc.h"

#include <kernel/heap.h>
#include <nulibc/string.h>

int xpt_alloc_dpc( xpt_dpc_info **dpc )
{
	SHOW_FLOW0( 3, "" );
	*dpc = kmalloc( sizeof( **dpc ));
	
	if( *dpc == NULL )
		return ERR_NO_MEMORY;

	memset( *dpc, 0, sizeof( **dpc ));
	
	return NO_ERROR;
}

int xpt_free_dpc( xpt_dpc_info *dpc )
{
	kfree( dpc );
	
	return NO_ERROR;
}

int xpt_schedule_dpc( xpt_bus_info *bus, xpt_dpc_info *dpc, /*int flags,*/
	void (*func)( void *arg ), void *arg )
{
	SHOW_FLOW( 3, "bus=%p, dpc=%p", bus, dpc );
	acquire_spinlock_irq( &bus->dpc_lock );
	
	dpc->func = func;
	dpc->arg = arg;
	
	if( !dpc->registered ) {
		dpc->registered = true;
		dpc->next = bus->dpc_list;
		bus->dpc_list = dpc;
	} else
		SHOW_FLOW0( 3, "already registered - ignored" );

	release_spinlock_irq( &bus->dpc_lock );
	
	sem_release( bus->start_service, 1 );
	
	return NO_ERROR;
}

bool xpt_check_exec_dpc( xpt_bus_info *bus )
{
	SHOW_FLOW( 3, "bus=%p, dpc_list=%p", bus, bus->dpc_list );
	acquire_spinlock_irq( &bus->dpc_lock );

	if( bus->dpc_list ) {
		xpt_dpc_info *dpc;
		void (*dpc_func)( void * );
		void *dpc_arg;
		
		dpc = bus->dpc_list;
		bus->dpc_list = dpc->next;
		
		dpc_func = dpc->func;
		dpc_arg = dpc->arg;
		dpc->registered = false;

		release_spinlock_irq( &bus->dpc_lock );
		
		dpc_func( dpc_arg );
		
		return true;
	} 
	
	release_spinlock_irq( &bus->dpc_lock );
	
	return false;
}