/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "sync.h"

#include <string.h>

#include "basic_prot.h"
#include "ide_sim.h"
#include "atapi.h"
#include "ata.h"
#include "queuing.h"

void ide_dpc( void *arg )
{
    ide_bus_info *bus = (ide_bus_info *)arg;
    ide_qrequest *qrequest;
    ide_device_info *device;

    SHOW_FLOW0( 3, "" );

    if ( bus->active_qrequest == NULL ) {
        SHOW_FLOW0( 3, "irq in idle mode - possible service request" );

        device = get_current_device( bus );

        if ( device == NULL ) {
            // got an interrupt from a non-existing device
            // either this is a spurious interrupt or there *is* a device
            // but haven't detected it - we better ignore it silently
            access_finished( bus, bus->first_device );

            xpt->cont_send_bus( bus->xpt_cookie );
            return;
        }

        // access_finished always checks the other device first, but as
        // we do have a service request, we negate the negation
        access_finished( bus, device->other_device );
        xpt->cont_send_bus( bus->xpt_cookie );
        return;

    } else {
        SHOW_FLOW0( 3, "continue command" );

        IDE_LOCK( bus );

        if ( bus->dpc_id != bus->wait_id ) {
            SHOW_FLOW0( 3, "waiting has been aborted - ignore irq" );

            // waiting must have been aborted in some way - we don't own bus,
            // so we are not allowed to do anything
            IDE_UNLOCK( bus );
            return;
        }

        IDE_UNLOCK( bus );

        // make sure timeout is not delayed on other processor
        // NEWOS BUG: current implementation doesn't guarantee that
        //            the timer function has finished execution on return!
        timer_cancel_event( &bus->timer );

        //++bus->cmd_id;

        qrequest = bus->active_qrequest;
        device = qrequest->device;

        if ( device->is_atapi )
            packet_dpc( qrequest );
        else {
            if ( device->DMA_enabled )
                ata_dpc_DMA( qrequest );
            else
                ata_dpc_PIO( qrequest );
        }
    }

    return;

    /*err:
        xpt->cont_send( bus->xpt_cookie );*/
}

int irq_handler( ide_bus_info *bus )
{
    SHOW_FLOW0( 3, "" );

    IDE_LOCK( bus );

    SHOW_FLOW0( 3, "1" );

    switch ( bus->state ) {
        case ide_state_async_waiting:
            SHOW_FLOW0( 3, "async waiting" );

            bus->state = ide_state_accessing;

            bus->dpc_id = ++bus->wait_id;

            bus->device_to_reconnect = NULL;

            IDE_UNLOCK( bus );

            xpt->schedule_dpc( bus->xpt_cookie, bus->irq_dpc,
                               ide_dpc, bus );

            return INT_RESCHEDULE;

        case ide_state_idle:
            SHOW_FLOW0( 3, "idle" );

            if ( bus->num_running_reqs == 0 ) {
                // spurious interrupt
                IDE_UNLOCK( bus );
                return INT_NO_RESCHEDULE;
            }

            bus->state = ide_state_accessing;

            bus->dpc_id = ++bus->wait_id;

            IDE_UNLOCK( bus );

            xpt->schedule_dpc( bus->xpt_cookie, bus->irq_dpc,
                               ide_dpc, bus );

            return INT_RESCHEDULE;

        case ide_state_sync_waiting:
            SHOW_FLOW0( 3, "sync waiting" );

            bus->state = ide_state_accessing;
            bus->sync_wait_timeout = false;

            IDE_UNLOCK( bus );

            {
                int res;

                // cancel timer manually, normally that's done by DPC

                // POSSIBLE RACE CONDITION:
                // timeout may be executed and delayed on other processor;
                // cancel_timer doesn't wait for it if in IRQ handler
                res = timer_cancel_event( &bus->timer );

                SHOW_FLOW( 3, "timeout canceled (%s)", strerror( res ));
            }

            sem_release_etc( bus->sync_wait_sem, 1, SEM_FLAG_NO_RESCHED );

            return INT_RESCHEDULE;

        case ide_state_accessing:
            SHOW_FLOW0( 3, "spurious IRQ - there is a command being executed" );

            IDE_UNLOCK( bus );
            return INT_NO_RESCHEDULE;

        default:
            SHOW_ERROR( 0, "BUG: unknown state (%i)", (int)bus->state );

            IDE_UNLOCK( bus );

            return INT_NO_RESCHEDULE;
    }
}

void cancel_irq_timeout( ide_bus_info *bus )
{
    IDE_LOCK( bus );
    ++bus->wait_id;
    bus->state = ide_state_accessing;
    IDE_UNLOCK( bus );

    timer_cancel_event( &bus->timer );
}


// bus must be locked
// new_state must be either sync_wait or async_wait
void start_waiting( ide_bus_info *bus, bigtime_t timeout, int new_state )
{
    int res;

    SHOW_FLOW( 3, "timeout=%Li", timeout );
    //IDE_LOCK( bus );

    //bus->irq_id = ++bus->cmd_id;
    bus->state = new_state;
    ++bus->wait_id;

    res = timer_set_event( timeout/*qrequest->request->cam_timeout > 0 ?
        qrequest->request->cam_timeout : IDE_STD_TIMEOUT*/,
                           TIMER_MODE_ONESHOT, &bus->timer );

    SHOW_FLOW( 3, "timer set (%s)", strerror( res ));

    IDE_UNLOCK( bus );
}

// bus must not be locked
void start_waiting_nolock( ide_bus_info *bus, bigtime_t timeout, int new_state )
{
    IDE_LOCK( bus );
    start_waiting( bus, timeout, new_state );
}

static void ide_timeout_dpc( void *arg )
{
    ide_bus_info *bus = (ide_bus_info *)arg;
    ide_qrequest *qrequest;

    IDE_LOCK( bus );
    if ( bus->dpc_id != bus->wait_id ) {
        IDE_UNLOCK( bus );
        return;
    }

    IDE_UNLOCK( bus );

    qrequest = bus->active_qrequest;

    set_sense( qrequest->device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_LUN_TIMEOUT );

    if ( qrequest->device->is_atapi )
        adjust_atapi_result( qrequest->device, qrequest );

    finish_reset_queue( qrequest );

    access_finished( bus, qrequest->device );
}

int ide_timeout( void *arg )
{
    ide_bus_info *bus = (ide_bus_info *)arg;
    //ide_qrequest *qrequest;

    SHOW_FLOW0( 3, "" );

    IDE_LOCK( bus );

    switch ( bus->state ) {
        case ide_state_async_waiting:
            SHOW_FLOW0( 3, "async" );

            bus->state = ide_state_accessing;

            bus->dpc_id = ++bus->wait_id;

            IDE_UNLOCK( bus );

            xpt->schedule_dpc( bus->xpt_cookie, bus->irq_dpc, /*qrequest->device->target_id, 0, 0,*/
                               ide_timeout_dpc, bus );

            return INT_RESCHEDULE;

        case ide_state_sync_waiting:
            SHOW_FLOW0( 3, "sync" );

            bus->state = ide_state_accessing;
            bus->sync_wait_timeout = true;

            IDE_UNLOCK( bus );

            sem_release_etc( bus->sync_wait_sem, 1, SEM_FLAG_NO_RESCHED );

            return INT_RESCHEDULE;

        case ide_state_accessing:
            SHOW_FLOW0( 3, "came too late - IRQ occured already" );

            IDE_UNLOCK( bus );
            return INT_NO_RESCHEDULE;

        default:
            SHOW_ERROR( 0, "BUG: unknown state (%i)", (int)bus->state );

            IDE_UNLOCK( bus );
            return INT_NO_RESCHEDULE;

    }
}

void init_synced_pc( ide_synced_pc *pc, ide_synced_pc_func func )
{
    pc->func = func;
    pc->registered = false;
}

void uninit_synced_pc( ide_synced_pc *pc )
{
    if ( pc->registered )
        panic( "Tried to clean up pending synced PC\n" );
}

//void access_finished( ide_device_info *device );

int schedule_synced_pc( ide_bus_info *bus, ide_synced_pc *pc, void *arg )
{
    SHOW_FLOW0( 3, "" );

    IDE_LOCK( bus );
    if ( pc->registered ) {
        SHOW_FLOW0( 3, "already registered" );
        return ERR_GENERAL;

    } else if ( bus->state != ide_state_idle ) {
        SHOW_FLOW0( 3, "adding to pending list" );
        pc->next = bus->synced_pc_list;
        bus->synced_pc_list = pc;
        pc->arg = arg;
        pc->registered = true;

        IDE_UNLOCK( bus );
        return NO_ERROR;

    } else {
        SHOW_FLOW0( 3, "exec immediately" );
        bus->state = ide_state_accessing;
        IDE_UNLOCK( bus );

        SHOW_FLOW0( 3, "go" );
        pc->func( bus, arg );

        SHOW_FLOW0( 3, "finished" );
        access_finished( bus, bus->first_device );

        SHOW_FLOW0( 3, "tell XPT about idle bus" );
        xpt->cont_send_bus( bus->xpt_cookie );
        return NO_ERROR;
    }
}



static void exec_synced_pcs( ide_bus_info *bus, ide_synced_pc *pc_list )
{
    ide_synced_pc *pc;

    for ( pc = pc_list; pc; pc = pc->next ) {
        pc->func( bus, pc->arg );
    }

    IDE_LOCK( bus );

    for ( pc = pc_list; pc; pc = pc->next ) {
        pc->registered = false;
    }

    IDE_UNLOCK( bus );
}

void access_finished( ide_bus_info *bus, ide_device_info *device )
{
    SHOW_FLOW( 3, "bus=%p, device=%p", bus, device );

    IDE_LOCK( bus );

    while ( 1 ) {
        ide_synced_pc *synced_pc_list;

        if ( device ) {
            if ( try_service( device ))
                return;
        }

        bus->state = ide_state_idle;

        if ( bus->synced_pc_list == NULL ) {
            //bus->state = ide_state_idle;
            IDE_UNLOCK( bus );
            return;
        }

        synced_pc_list = bus->synced_pc_list;
        bus->synced_pc_list = NULL;

        IDE_UNLOCK( bus );

        exec_synced_pcs( bus, synced_pc_list );
    }
}
