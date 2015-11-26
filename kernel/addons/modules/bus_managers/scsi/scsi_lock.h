/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __SCSI_LOCK_H__
#define __SCSI_LOCK_H__

#include <kernel/smp.h>
#include <kernel/int.h>
#include <kernel/lock.h>
#include <kernel/sem.h>

#define LOCK_MAX_COUNT 2000000000

typedef struct spinlock_irq {
    spinlock_t lock;
} spinlock_irq;

static inline void spinlock_irq_init( spinlock_irq *lock )
{
    lock->lock = 0;
}

static inline void acquire_spinlock_irq( spinlock_irq *lock )
{
    int_disable_interrupts();

    acquire_spinlock( &lock->lock );
}

static inline void release_spinlock_irq( spinlock_irq *lock )
{
    release_spinlock( &lock->lock );
    int_restore_interrupts();
}


// general: upon lock, you have to lock a global lock and check that
// the referred object is registered
typedef struct ref_lock {
    int nonexcl_count;
    sem_id nonexcl_lock;

    int excl_count;
    sem_id excl_lock;
    sem_id excl_mutex_lock;

    sem_id destroy_lock;
    bool destroying;
} ref_lock;


// must own global_lock
static inline bool ref_lock_nonexcl_lock( ref_lock *lock, mutex *global_lock )
{
    int non_excl_count;

    non_excl_count = atomic_add( &lock->nonexcl_count, 1 );
    mutex_unlock( global_lock );

    if ( non_excl_count >= 0 )
        return true;

    sem_acquire( lock->nonexcl_lock, 1 );

    if ( !lock->destroying )
        return true;

    sem_release( lock->destroy_lock, 1 );
    return false;
}

static inline void ref_lock_nonexcl_unlock( ref_lock *lock )
{
    if ( atomic_add( &lock->nonexcl_count, -1 ) < 0 )
        sem_release_etc( lock->excl_lock, 1, SEM_FLAG_NO_RESCHED );
}

// must own global_lock
bool ref_lock_excl_lock( ref_lock *lock, mutex *global_lock );
void ref_lock_excl_unlock( ref_lock *lock );
// must own exclusive lock and have made sure that noone will try
// to lock upon call
void ref_lock_destroying_object( ref_lock *lock );

int ref_lock_init( ref_lock *lock );
void ref_lock_uninit( ref_lock *lock );

#endif
