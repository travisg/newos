/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __SYNC_H__
#define __SYNC_H__

void start_waiting( ide_bus_info *bus, bigtime_t timeout, int new_state );
void start_waiting_nolock( ide_bus_info *bus, bigtime_t timeout, int new_state );
void cancel_irq_timeout( ide_bus_info *bus );

int schedule_synced_pc( ide_bus_info *bus, ide_synced_pc *pc, void *arg );
void init_synced_pc( ide_synced_pc *pc, ide_synced_pc_func func );
void uninit_synced_pc( ide_synced_pc *pc );

void ide_dpc( void *arg );
void access_finished( ide_bus_info *bus, ide_device_info *device );

int irq_handler( ide_bus_info *bus );
int ide_timeout( void *arg );

#endif
