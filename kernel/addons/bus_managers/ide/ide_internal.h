/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __IDE_INTERNAL_H__
#define __IDE_INTERNAL_H__

#include <kernel/bus/ide/ide.h>
#include <kernel/timer.h>
#include <kernel/bus/scsi/CAM.h>
#include <kernel/smp.h>
#include <kernel/int.h>
#include "ide_device_infoblock.h"

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "IDE -- "

#include <kernel/debug.h>
#include <kernel/debug_ext.h>


#define IDE_STD_TIMEOUT 10000000
#define IDE_RELEASE_TIMEOUT 10000000


typedef struct ide_bus_info ide_bus_info;

typedef void (*ide_synced_pc_func)( ide_bus_info *bus, void *arg );

typedef struct ide_synced_pc {
	struct ide_synced_pc *next;
	ide_synced_pc_func func;
	void *arg;
	bool registered;
} ide_synced_pc;

typedef enum {
	ide_irq_state_ignore = 0,
	ide_irq_state_cmd = 1,
	ide_irq_state_bus_released = 2
} ide_irq_state;

typedef struct ide_device_info {
	uint8 use_LBA : 1;
	uint8 use_48bits : 1;
	uint8 is_atapi : 1;
	uint8 CQ_supported : 1;
	uint8 CQ_enabled : 1;
	uint8 DMA_supported : 1;
	uint8 DMA_enabled : 1;

	uint8 queue_depth;

	int DMA_failures;
	int CQ_failures;
	struct ide_bus_info *bus;
	struct ide_qrequest *qreq_array;

	uint32 combined_sense;
	struct ide_qrequest *free_qrequests;
	int num_running_reqs;

	void (*exec_io)( struct ide_device_info *device, struct ide_qrequest *qrequest );

	int is_device1;
	int target_id;

	ide_task_file tf;
	ide_reg_mask tf_param_mask;

	struct {
		uint8 packet_irq : 1;
		bigtime_t packet_irq_timeout;
	} atapi;

	bool no_reconnect_timeout;

	struct timer_event reconnect_timer;
	ide_synced_pc reconnect_timeout_synced_pc;
	xpt_dpc_cookie reconnect_timeout_dpc;

	struct ide_device_info *other_device;

	int left_sg_elem;
	struct sg_elem *cur_sg_elem;
	int cur_sg_ofs;

	int left_blocks;

	uint8 packet[12];

	int device_type;

	struct sg_elem *saved_sg_list;
	uint16 saved_sglist_cnt;
	uint32 saved_dxfer_len;

	char *buffer;
	struct sg_elem *buffer_sg_list;
	size_t buffer_sglist_cnt;
	size_t buffer_size;

	uint64 total_sectors;
	ide_device_infoblock infoblock;

	bool has_odd_byte;
	int odd_byte;
} ide_device_info;

typedef struct ide_qrequest {
	struct ide_qrequest *next;
	ide_device_info *device;
	uint8 is_write : 1;
	uint8 running : 1;
	uint8 uses_dma : 1;
	uint8 packet_irq : 1;
	uint8 queuable : 1;
	uint8 tag;
	CCB_SCSIIO *request;
} ide_qrequest;

typedef enum {
	ide_state_idle,
	ide_state_accessing,
	ide_state_async_waiting,
	ide_state_sync_waiting,
} ide_bus_state;

struct ide_bus_info {
	spinlock_t lock;
	bool disconnected;
	int num_running_reqs;
	ide_controller_interface *controller;
	ide_channel_cookie channel;
	xpt_bus_cookie xpt_cookie;
	ide_qrequest *active_qrequest;
	int wait_id, dpc_id;
	struct timer_event timer;
	ide_bus_state state;
	ide_device_info *device_to_reconnect;
	ide_synced_pc *synced_pc_list;
	xpt_dpc_cookie irq_dpc;
	ide_device_info *active_device;
	sem_id sync_wait_sem;
	bool sync_wait_timeout;

	ide_device_info *devices[2];

	ide_synced_pc scan_bus_syncinfo;
	sem_id scan_device_sem;

	ide_synced_pc disconnect_syncinfo;

	ide_device_info *first_device;

	char controller_name[HBA_ID];
	ide_controller_params controller_params;
	uchar path_id;
};

#define IDE_LOCK( bus ) { \
	int_disable_interrupts(); \
	acquire_spinlock( &bus->lock ); \
}

#define IDE_UNLOCK( bus ) { \
	release_spinlock( &bus->lock ); \
	int_restore_interrupts(); \
}

#endif
