/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __XPT_INTERNAL_H__
#define __XPT_INTERNAL_H__


#include <kernel/kernel.h>
#include <kernel/bus/scsi/CAM.h>
#include <kernel/bus/scsi/scsi_cmds.h>
#include <kernel/generic/locked_pool.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "SCSI -- "

#include <kernel/debug.h>
#include <kernel/debug_ext.h>

#include "scsi_lock.h"


#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif


// XXX the difference between path and bus is unclear; I call all structures
//     and functions "bus" and its index path_id

// path is stored as an uchar and we want to cover the whole uchar range
// to avoid the range check (id 255 is xpt itself)!
#define MAX_PATH_ID 255
#define MAX_TARGET_ID 31
#define MAX_LUN_ID 7


typedef struct xpt_dpc_info {
    struct xpt_dpc_info *next;
    bool registered;

    void (*func)( void * );
    void *arg;
} xpt_dpc_info;


typedef struct xpt_bus_info {
    int lock_count;
    int blocked_by_sim;
    bool shutting_down;
    bool TCQ_support;
    bool do_rescan;
    bool sim_overflow;

    uchar path_id;

    cam_sim_cookie sim_cookie;

    CCB_SCSIIO *resubmitted_req;

    thread_id service_thread;
    sem_id start_service;

    //spinlock_t lock;
    mutex mutex;
    //int prev_irq_state;

    //ref_lock lock;
    sem_id scan_lun_lock;

    cam_sim_interface *interface;

    spinlock_irq dpc_lock;

    xpt_dpc_info *dpc_list;
    struct xpt_device_info *waiting_devices;

    locked_pool_cookie ccb_pool;
    int ref_count;
    bool disconnected;

    struct xpt_device_info *global_device;

    struct xpt_target_info *targets[MAX_TARGET_ID + 1];
} xpt_bus_info;

typedef struct xpt_target_info {
    int num_devices;
    struct xpt_device_info *devices[MAX_LUN_ID + 1];
} xpt_target_info;

typedef struct xpt_device_info {
    struct xpt_device_info *next_waiting;
    struct xpt_device_info *prev_waiting;

    int in_wait_queue;

    int lock_count;     // number of queued reqs + blocked_by_sim + sim_overflow
    int blocked_by_sim;
    int sim_overflow;
    CCB_SCSIIO *queued_reqs;

    /*ref_lock lock;*/

    int64 last_sort;

    xpt_bus_info *bus;
    int temporary;      // access to this must be atomically!
    uchar target_id;
    uchar target_lun;
    int ref_count;

    scsi_res_inquiry inquiry_data;
} xpt_device_info;

typedef struct xpt_periph_info {
    struct xpt_periph_info *next, *prev;
    cam_periph_interface *interface;
    cam_periph_cookie periph_cookie;
} xpt_periph_info;

enum {
    XPT_STATE_FREE = 0,
    XPT_STATE_INWORK = 1,
    XPT_STATE_QUEUED = 2,
    XPT_STATE_SENT = 3,
    XPT_STATE_FINISHED = 5,
} xpt_ccb_state;


bool xpt_shutting_down;
recursive_lock registration_lock;
locked_pool_interface *locked_pool;

int xpt_driver_sim_init( void );
int xpt_driver_sim_uninit( void );

#endif
