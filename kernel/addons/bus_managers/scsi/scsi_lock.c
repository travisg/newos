/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "scsi_lock.h"

#include <string.h>

bool ref_lock_excl_lock( ref_lock *lock, mutex *global_lock )
{
	int excl_count, nonexcl_count;
	
	excl_count = atomic_add( &lock->excl_count, 1 );
	mutex_unlock( global_lock );
	
	if( excl_count > 0 ) {
		sem_acquire( lock->excl_mutex_lock, 1 );
		
		if( lock->destroying ) {
			sem_release( lock->destroy_lock, 1 );
			return false;
		}
	}
	
	nonexcl_count = atomic_add( &lock->nonexcl_count, -LOCK_MAX_COUNT );
	if( nonexcl_count > 0 )
		sem_acquire( lock->excl_lock, nonexcl_count );
		
	return true;
}


void ref_lock_excl_unlock( ref_lock *lock )
{
	int nonexcl_count, excl_count;
	
	nonexcl_count = atomic_add( &lock->nonexcl_count, LOCK_MAX_COUNT ) + LOCK_MAX_COUNT;
	
	if( nonexcl_count > 0 )
		sem_release( lock->nonexcl_lock, nonexcl_count );

	excl_count = atomic_add( &lock->excl_count, -1 ) - 1;
	
	if( excl_count > 0 )
		sem_release( lock->excl_mutex_lock, 1 );
}


void ref_lock_destroying_object( ref_lock *lock )
{
	lock->destroying = true;
	
	sem_release( lock->nonexcl_lock, lock->nonexcl_count + LOCK_MAX_COUNT );
	sem_release( lock->excl_lock, lock->excl_count - 1 );
	
	sem_acquire( lock->destroy_lock, 
		lock->nonexcl_count + LOCK_MAX_COUNT + lock->excl_count - 1 );
}


int ref_lock_init( ref_lock *lock )
{
	memset( lock, 0, sizeof( lock ));
	
	lock->nonexcl_count = 0;
	lock->destroying = false;
	lock->excl_lock = sem_create( 0, "excl_lock" );
	lock->nonexcl_lock = sem_create( 0, "nonexcl_lock" );
	lock->destroy_lock = sem_create( 0, "destroy_lock" );
	lock->excl_mutex_lock = sem_create( 0, "excl_mutex_lock" );
	
	if( lock->excl_mutex_lock < 0 || lock->excl_lock < 0 || 
		lock->nonexcl_lock < 0 || lock->destroy_lock < 0 ) 
	{
		if( lock->excl_lock > 0 )
			sem_delete( lock->excl_lock );
		if( lock->nonexcl_lock > 0 )
			sem_delete( lock->nonexcl_lock );
		if( lock->destroy_lock > 0 )
			sem_delete( lock->destroy_lock );
		if( lock->excl_mutex_lock > 0 )
			sem_delete( lock->excl_mutex_lock );

		return ERR_NO_MEMORY;
	}
	
	return NO_ERROR;
}

void ref_lock_uninit( ref_lock *lock )
{
	sem_delete( lock->excl_lock );
	sem_delete( lock->nonexcl_lock );
	sem_delete( lock->destroy_lock );
	sem_delete( lock->excl_mutex_lock );
}
