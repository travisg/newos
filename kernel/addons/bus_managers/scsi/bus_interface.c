/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "bus_interface.h"

#include <kernel/debug.h>
#include <kernel/module.h>

#include "xpt_io.h"
#include "dpc.h"
#include "blocking.h"
#include "async.h"
#include "bus_mgr.h"

cam_for_sim_interface sim_interface =
{
	xpt_done,
	
	xpt_alloc_dpc,
	xpt_free_dpc,
	xpt_schedule_dpc,
	
	xpt_block_bus,
	xpt_unblock_bus,
	xpt_block_device,
	xpt_unblock_device,

	xpt_cont_send_bus,
	xpt_cont_send_device,
	
	xpt_call_async,
	xpt_flush_async,
	
	xpt_register_SIM,
	xpt_unregister_SIM
};


module_header xpt_sim_module = {
	CAM_FOR_SIM_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,
	&sim_interface,
	
	xpt_driver_sim_init,
	xpt_driver_sim_uninit
};
