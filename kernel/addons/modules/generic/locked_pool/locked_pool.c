/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include <kernel/kernel.h>
#include <kernel/ktypes.h>

#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/module.h>
#include <kernel/debug.h>
#include <kernel/generic/locked_pool.h>
#include <string.h>

#define debug_level_flow 0
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "LOCKED_POOL -- "

#include <kernel/debug_ext.h>


typedef struct locked_pool {
	mutex mutex;
	int free_blocks;
	int diff_locks;
	int num_waiting;
	sem_id enlarger_sem;
	sem_id sem;
	void *free_list;
	int next_ofs;
	char *name;
	size_t header_size;
	struct chunk_header *chunks;
	size_t block_size;
	int wiring_flags;
	bool shutting_down;
	int min_free_blocks;
	int num_blocks;
	int max_blocks;
	int enlarge_by;
	size_t alignment;
	thread_id enlarger_thread;
	locked_pool_alloc_hook alloc_hook;
	locked_pool_free_hook free_hook;
	void *hook_arg;
} locked_pool;

typedef struct chunk_header {
	struct chunk_header *next;
	region_id region;
	int num_blocks;
} chunk_header;

#define NEXT_PTR( pool, a ) ((void **)(((char *)a) + pool->next_ofs))


static void *pool_alloc( locked_pool *pool )
{
	void *block;
	int num_to_lock;
	
	SHOW_FLOW0( 3, "" );
	
	mutex_lock( &pool->mutex );
	
	--pool->free_blocks;
	
	if( pool->free_blocks > 0 ) {
		++pool->diff_locks;

		SHOW_FLOW( 3, "freeblocks=%i, diff_locks=%i, free_list=%p",
			pool->free_blocks, pool->diff_locks, pool->free_list );

		block = pool->free_list;
		pool->free_list = *NEXT_PTR( pool, block );
		
		SHOW_FLOW( 3, "new free_list=%p", pool->free_list );
		
		mutex_unlock( &pool->mutex );
		return block;
	}
	
	++pool->num_waiting;
	num_to_lock = pool->diff_locks+1;
	pool->diff_locks = 0;
	
	SHOW_FLOW( 3, "locking %i times (%i waiting allocs)",
		num_to_lock, pool->num_waiting );
	
	mutex_unlock( &pool->mutex );	
	
	sem_release_etc( pool->enlarger_sem, 1, SEM_FLAG_NO_RESCHED );
	sem_acquire( pool->sem, num_to_lock );
	
	mutex_lock( &pool->mutex );
	
	--pool->num_waiting;
	
	SHOW_FLOW( 3, "continuing alloc (%i free blocks)",
		pool->free_blocks );
	
	block = pool->free_list;
	pool->free_list = *NEXT_PTR( pool, block );
	
	mutex_unlock( &pool->mutex );
	return block;
}

static void pool_free( locked_pool *pool, void *block )
{
	int num_to_unlock;
	
	SHOW_FLOW0( 3, "" );
	
	mutex_lock( &pool->mutex );
	
	*NEXT_PTR( pool, block ) = pool->free_list;
	pool->free_list = block;
	
	++pool->free_blocks;
	--pool->diff_locks;
	
	SHOW_FLOW( 3, "freeblocks=%i, diff_locks=%i, free_list=%p",
		pool->free_blocks, pool->diff_locks, pool->free_list );
	
	if( pool->num_waiting == 0 ) {
		//SHOW_FLOW0( 3, "leaving" );
		mutex_unlock( &pool->mutex );
		//SHOW_FLOW0( 3, "done" );
		return;
	}
	
	num_to_unlock = -pool->diff_locks;
	pool->diff_locks = 0;
	
	SHOW_FLOW( 3, "unlocking %i times (%i waiting allocs)",
		num_to_unlock, pool->num_waiting );
	
	mutex_unlock( &pool->mutex );
	
	sem_release( pool->sem, num_to_unlock );
	return;
}

static int enlarge_pool( locked_pool *pool, int num_blocks )
{
	void **next;
	int i;
	int res;
	region_id region;
	chunk_header *chunk;
	size_t chunk_size;
	int num_to_unlock;
	void *block, *last_block;
	
	SHOW_FLOW0( 3, "" );
	
	chunk_size = num_blocks * pool->block_size + pool->header_size;
	
	res = region = vm_create_anonymous_region( 
		vm_get_kernel_aspace_id(), pool->name,
		(void **)&chunk, REGION_ADDR_ANY_ADDRESS, chunk_size,
		pool->wiring_flags, LOCK_KERNEL | LOCK_RW );
		
	if( res < 0 ) {
		SHOW_ERROR( 3, "cannot enlarge pool (%s)", strerror( res ));
		return res;
	}
	
	chunk->region = region;
	chunk->num_blocks = num_blocks;
	
	next = NULL;
	
	last_block = (char *)chunk + pool->header_size + 
		(num_blocks-1) * pool->block_size;
		
	for( i = 0, block = last_block; i < num_blocks;
		 ++i, block = (char *)block - pool->block_size ) 
	{
		if( pool->alloc_hook ) {
			if( (res = pool->alloc_hook( block, pool->hook_arg )) < 0 )
				break;
		}
			
		*NEXT_PTR( pool, block ) = next;		
		next = block;
	}
	
	if( i < num_blocks ) {
		int j;
		
		for( block = last_block, j = 0; j < i; 
			 ++j, block = (char *)block - pool->block_size )
		{
			if( pool->free_hook ) 
				pool->free_hook( block, pool->hook_arg );
		}
		
		vm_delete_region( vm_get_kernel_aspace_id(), chunk->region );
		
		return res;
	}
	
	mutex_lock( &pool->mutex );
	
	*NEXT_PTR( pool, last_block ) = pool->free_list;
	pool->free_list = next;
	
	chunk->next = pool->chunks;
	pool->chunks = chunk;
	
	pool->num_blocks += num_blocks;
	
	pool->free_blocks += num_blocks;
	pool->diff_locks -= num_blocks;
	
	num_to_unlock = -pool->diff_locks;
	pool->diff_locks = 0;
	
	SHOW_FLOW( 3, "done - num_blocks=%i, free_blocks=%i, num_waiting=%i, num_to_unlock=%i", 
		pool->num_blocks, pool->free_blocks, pool->num_waiting, num_to_unlock );

	mutex_unlock( &pool->mutex );
	
	sem_release( pool->sem, num_to_unlock );	
	
	return NO_ERROR;
}

static int enlarger_threadproc( void *arg )
{
	locked_pool *pool = (locked_pool *)arg;
	
	while( 1 ) {
		int num_free;
		
		sem_acquire( pool->enlarger_sem, 1 );
	
		if( pool->shutting_down )
			break;
			
		// this mutex is probably not necessary (at least on 80x86)
		// but I'm not sure about atomicity of other architectures
		// (anyway - this routine is not performance critical)
		mutex_lock( &pool->mutex );
		num_free = pool->free_blocks;
		mutex_unlock( &pool->mutex );

		if( num_free > pool->min_free_blocks )
			continue;
			
		if( pool->num_blocks < pool->max_blocks )
			enlarge_pool( pool, pool->enlarge_by );
	}
	
	return 0;
}

static void free_chunks( locked_pool *pool )
{
	chunk_header *chunk, *next;
	
	for( chunk = pool->chunks; chunk; chunk = next ) {
		int i;
		void *block, *last_block;
		
		next = chunk->next;
		
		last_block = (char *)chunk + pool->header_size + 
			(chunk->num_blocks-1) * pool->block_size;
		
		for( i = 0, block = last_block; i < pool->num_blocks;
			 ++i, block = (char *)block - pool->block_size ) 
		{
			if( pool->free_hook )
				pool->free_hook( block, pool->hook_arg );
				
			*NEXT_PTR( pool, block ) = next;		
			next = block;
		}
		
		vm_delete_region( vm_get_kernel_aspace_id(), chunk->region );
	}
	
	pool->chunks = NULL;
}

static locked_pool *init_pool( 
	int block_size, int alignment, int next_ofs,
	int chunk_size, int max_blocks, int min_free_blocks, 
	const char *name, int wiring_flags,
	locked_pool_alloc_hook alloc_hook, 
	locked_pool_free_hook free_hook, void *hook_arg )
{
	locked_pool *pool;
	int res;
	
	SHOW_FLOW0( 3, "" );
	
	pool = kmalloc( sizeof( *pool ));
	if( pool == NULL )
		return NULL;
		
	if( (res = mutex_init( &pool->mutex, "xpt_ccb_mutex" )) < 0 )
		goto err;
		
	if( (res = pool->sem = sem_create( 0, "locked_pool" )) < 0 )
		goto err1;
		
	if( (res = pool->enlarger_sem = sem_create( 0, "locked_pool_enlarger" )) < 0 )
		goto err2;
		
	if( (pool->name = kstrdup( name )) == NULL ) {
		res = ERR_NO_MEMORY;
		goto err3;
	}
	
	SHOW_FLOW0( 3, "1" );
		
	pool->block_size = (block_size + pool->alignment) & ~pool->alignment;
	pool->alignment = alignment;
	pool->next_ofs = next_ofs;
	pool->wiring_flags = wiring_flags;
	pool->header_size = max( 
		(sizeof( chunk_header ) + pool->alignment) & ~pool->alignment, 
		pool->alignment + 1 );
	pool->enlarge_by = 
		(((chunk_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - pool->header_size) 
		/ pool->block_size;
	pool->max_blocks = max_blocks;
	pool->min_free_blocks = min_free_blocks;
	pool->free_blocks = 0;
	pool->num_blocks = 0;
	pool->diff_locks = 0;
	pool->num_waiting = 0;
	pool->free_list = NULL;
	pool->alloc_hook = alloc_hook;
	pool->free_hook = free_hook;
	pool->hook_arg = hook_arg;
	pool->chunks = NULL;
	
	SHOW_INFO( 3, "block_size=%i, alignment=%i, next_ofs=%i, wiring_flags=%i, header_size=%i, enlarge_by=%i",
		(int)pool->block_size, (int)pool->alignment, (int)pool->next_ofs, pool->wiring_flags, (int)pool->header_size, pool->enlarge_by );

	if( min_free_blocks > 0 ) {
		if( (res = enlarge_pool( pool, pool->enlarge_by )) < 0 )
			goto err4;
	}
		
	if( (res = pool->enlarger_thread = 
		thread_create_kernel_thread( "locked_pool_enlarger", 
		enlarger_threadproc, pool )) < 0 )
		goto err5;
		
	pool->shutting_down = false;
	
	thread_resume_thread( pool->enlarger_thread );
	
	return pool;
	
err5:
	free_chunks( pool );	
err4:
	kfree( pool->name );
err3:
	sem_delete( pool->enlarger_sem );
err2:
	sem_delete( pool->sem );
err1:
	mutex_destroy( &pool->mutex );
err:
	kfree( pool );
	return NULL;
}

static void uninit_pool( locked_pool *pool )
{
	int retcode;
	
	pool->shutting_down = true;
	thread_wait_on_thread( pool->enlarger_thread, &retcode );
	
	free_chunks( pool );
	
	kfree( pool->name );
	sem_delete( pool->enlarger_sem );
	sem_delete( pool->sem );
	mutex_destroy( &pool->mutex );
	
	kfree( pool );
}

static int dummy_init( void )
{
	return NO_ERROR;
}

static int dummy_uninit( void )
{
	return NO_ERROR;
}


locked_pool_interface interface = {
	pool_alloc, pool_free,
	init_pool, uninit_pool,
};

struct module_header module = {
	LOCKED_POOL_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,
	&interface,
	
	dummy_init,
	dummy_uninit
};

module_header *modules[] = {
	&module,
	NULL
};
