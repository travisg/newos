/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __DPC_H__
#define __DPC_H__

int xpt_alloc_dpc( xpt_dpc_info **dpc );
int xpt_free_dpc( xpt_dpc_info *dpc );
bool xpt_check_exec_dpc( xpt_bus_info *bus );

int xpt_schedule_dpc( xpt_bus_info *bus, xpt_dpc_info *dpc, /*int flags,*/
	void (*func)( void *arg ), void *arg );

#endif
