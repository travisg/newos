#include <kernel/dev/blkman.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/vm.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <string.h>
#include <kernel/module.h>
#include <kernel/partitions/partitions.h>
#include <kernel/generic/locked_pool.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "BLKMAN -- "

#include <kernel/debug_ext.h>

typedef struct blkman_device_info {
	blkdev_interface *interface;
	blkdev_cookie dev_cookie;
	mutex lock;
	phys_vecs *free_phys_vecs;
	sem_id phys_vec_avail;
	blkdev_params params;
	char *name;
	locked_pool_cookie phys_vecs_pool;

	part_device_cookie part_mngr_cookie;
} blkman_device_info;

typedef struct handle_info {
	blkman_device_info *device;
	blkdev_handle_cookie handle_cookie;
} blkman_handle_info;

extern struct dev_calls dev_interface;

uint blkman_buffer_size;
mutex blkman_buffer_mutex;
struct iovec blkman_buffer_vec[1];
addr_t blkman_buffer_phys;
char *blkman_buffer;
phys_vecs blkman_buffer_phys_vec;
region_id blkman_buffer_region;

partitions_manager *part_mngr;
locked_pool_interface *locked_pool;

/*static phys_vecs *blkman_alloc_phys_vecs( blkman_device_info *device );
static void blkman_free_phys_vecs( blkman_device_info *device, phys_vecs *vec );*/
static int devfs_unpublish_device( const char *name );

static int blkman_register_dev( blkdev_interface *interface, blkdev_cookie cookie,
	const char *name, blkman_dev_cookie *blkman_cookie, blkdev_params *params )
{
	blkman_device_info *device;
	int res;

	SHOW_FLOW0( 3, "" );

	device = kmalloc( sizeof( *device ));
	if( device == NULL )
		return ERR_NO_MEMORY;

	device->name = kstrdup( name );
	if( device->name == NULL ) {
		res = ERR_NO_MEMORY;
		goto err1;
	}

	res = mutex_init( &device->lock, "blkdev_mutex" );
	if( res != NO_ERROR )
		goto err2;

	device->phys_vecs_pool = locked_pool->init(
		params->max_sg_num * sizeof( phys_vec ),
		sizeof( phys_vec ) - 1,
		0, 16*1024, 32, 0, name, REGION_WIRING_WIRED_CONTIG,
		NULL, NULL, NULL );

	if( device->phys_vecs_pool == NULL ) {
		res = ERR_NO_MEMORY;
		goto err3;
	}

	device->interface = interface;
	device->dev_cookie = cookie;
	device->params = *params;

	res = devfs_publish_device( name, device, &dev_interface );
	if( res != NO_ERROR )
		goto err4;

	/*res = part_mngr->add_blkdev( name, &device->part_mngr_cookie );
	if( res != NO_ERROR )
		goto err5;*/

	SHOW_FLOW0( 3, "done" );

	return NO_ERROR;

	devfs_unpublish_device( name );
err4:
	locked_pool->uninit( device->phys_vecs_pool );
err3:
	mutex_destroy( &device->lock );
err2:
	kfree( device->name );
err1:
	kfree( device );
	return res;
}

static int blkman_unregister_dev( blkman_device_info *device )
{
	/*part_mngr->remove_blkdev( device->part_mngr_cookie );*/
	devfs_unpublish_device( device->name );
	locked_pool->uninit( device->phys_vecs_pool );
	mutex_destroy( &device->lock );
	kfree( device->name );
	kfree( device );

	return NO_ERROR;
}


static int blkman_open( blkman_device_info *device, blkman_handle_info **res_handle )
{
	blkman_handle_info *handle;
	int res;

	handle = kmalloc( sizeof( *handle ));

	if( handle == NULL )
		return ERR_NO_MEMORY;

	handle->device = device;

	res = device->interface->open( device->dev_cookie, &handle->handle_cookie );
	if( res != NO_ERROR )
		goto err;

	return NO_ERROR;

err:
	kfree( handle );
	return res;
}

static int blkman_close( blkman_handle_info *handle )
{
	return NO_ERROR;
}


static int blkman_freecookie( blkman_handle_info *handle )
{
	blkman_device_info *device = handle->device;

	device->interface->close( handle->handle_cookie );

	kfree( handle );
	return NO_ERROR;
}

static int blkman_seek( blkman_handle_info *handle, off_t pos, seek_type st )
{
	return ERR_NOT_ALLOWED;
}

static int blkman_ioctl( blkman_handle_info *handle, int op, void *buf, size_t len)
{
	blkman_device_info *device = handle->device;

	return device->interface->ioctl( handle->handle_cookie, op, buf, len );
}

#define ROUND_PAGE_DOWN( addr_t ) ( (addr) & ~(PAGE_SIZE - 1))
#define ROUND_PAGE_UP( addr ) ( ((addr) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

// unused
#if 0
static size_t blkman_range2pagenum( addr_t start, size_t len )
{
	addr_t endof_first_page, beginof_last_page;

	endof_first_page = ROUND_PAGE_UP( start );
	beginof_last_page = ROUND_PAGE_DOWN(start + len);

	return endof_first_page <= beginof_last_page ?
		(beginof_last_page - endof_first_page) / PAGE_SIZE + 2 :
		1;
}
#endif

int vm_get_iovec_page_mapping(
	iovec *vec, size_t vec_count, size_t vec_offset, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len );

static int blkman_map_iovecs(
	iovec *vec, size_t vec_count, size_t vec_offset, size_t len,
	phys_vecs *map, size_t max_phys_entries,
	size_t dma_boundary, bool dma_boundary_solid )
{
	int res;
	size_t total_len;
	size_t cur_idx;

	if( (res = vm_get_iovec_page_mapping( vec, vec_count, vec_offset, len,
		map->vec, max_phys_entries, &map->num, &map->total_len )) != NO_ERROR )
		return res;

	if( dma_boundary == 0 )
		return NO_ERROR;

	total_len = 0;

	for( cur_idx = 0; cur_idx < map->num; ++cur_idx ) {
		addr_t dma_end, dma_len;

		dma_end = ((addr_t)map->vec[cur_idx].start + map->vec[cur_idx].len
			+ dma_boundary - 1) & ~(dma_boundary - 1);
		dma_len = dma_end - map->vec[cur_idx].len;

		if( dma_len != map->vec[cur_idx].len ) {
			if( dma_boundary_solid )
				break;
			else {
				map->num = min( map->num + 1, max_phys_entries );
				memcpy( &map->vec[cur_idx + 1], &map->vec[cur_idx],
					map->num - 1 - cur_idx );

				map->vec[cur_idx].len = dma_len;
				map->vec[cur_idx + 1].start += dma_len;
				map->vec[cur_idx + 1].len -= dma_len;
			}
		}

		total_len += map->vec[cur_idx].len;
	}

	map->total_len = total_len;
	return NO_ERROR;
}

static bool blkman_check_alignment( struct iovec *vecs, uint num_vecs,
	size_t vec_offset, uint alignment )
{
	if( alignment == 0 )
		return true;

	for( ; num_vecs > 0; ++vecs, --num_vecs )
	{
		if( (((addr_t)vecs->start + vec_offset) & alignment) != 0 )
			return false;

		if( (((addr_t)vecs->start + vecs->len) & alignment) != 0 )
			return false;

		vec_offset = 0;
	}

	return true;
}

#define VM_LOCK_DMA_TO_MEMORY 1
#define VM_LOCK_DMA_FROM_MEMORY 2

int vm_lock_range( addr_t vaddr, size_t len, int flags );
int vm_unlock_range( addr_t vaddr, size_t len );


static int blkman_lock_iovecs( struct iovec *vecs, uint num_vecs,
	size_t vec_offset, size_t len, int flags )
{
	size_t orig_len = len;

	for( ; len > 0; ++vecs, --num_vecs ) {
		size_t lock_len;

		lock_len = min( vecs->len - vec_offset, len );

		if( vm_lock_range( (addr_t)vecs->start + vec_offset,
			lock_len, flags ) != NO_ERROR )
			break;

		len -= lock_len;
		vec_offset = 0;
	}

	return orig_len - len;
}

static int blkman_unlock_iovecs( struct iovec *vecs, uint num_vecs,
	size_t vec_offset, size_t len )
{
	size_t orig_len = len;

	for( ; len > 0; ++vecs, --num_vecs ) {
		size_t lock_len;

		lock_len = min( vecs->len - vec_offset, len );

		vm_unlock_range( (addr_t)vecs->start + vec_offset, lock_len );

		len -= lock_len;
		vec_offset = 0;
	}

	return orig_len - len;
}

static void blkman_copy_buffer( char *buffer, struct iovec *vecs, uint num_vecs,
	size_t vec_offset, size_t len, bool to_buffer )
{
	for( ; len > 0 && num_vecs > 0; ++vecs, --num_vecs ) {
		size_t bytes;

		bytes = min( len, vecs->len - vec_offset );

		if( to_buffer )
			memcpy( buffer, vecs->start + vec_offset, bytes );
		else
			memcpy( vecs->start + vec_offset, buffer, bytes );

		buffer += bytes;
		vec_offset = 0;
	}
}

static inline ssize_t blkman_readwrite( blkman_handle_info *handle, struct iovec *vec, int vec_count,
	off_t pos, ssize_t len, bool need_locking, bool write )
{
	blkman_device_info *device = handle->device;
	size_t block_size;
	uint64 capacity;
	int res = NO_ERROR;

	int orig_len = len;
	size_t vec_offset;

	phys_vecs *phys_vecs;

	mutex_lock( &device->lock );
	block_size = device->params.block_size;
	capacity = device->params.capacity;
	mutex_unlock( &device->lock );

	if( capacity == 0 ) {
		res = ERR_DEV_NO_MEDIA;
		goto err;
	}

	if( block_size == 0 ) {
		res = ERR_DEV_GENERAL;
		goto err;
	}

	phys_vecs = locked_pool->alloc( handle->device->phys_vecs_pool );

	vec_offset = 0;

	while( len > 0 ) {
		//off_t block_pos;
		uint64 block_pos;
		uint block_ofs;
		bool need_buffer;
		size_t cur_len;

		size_t cur_blocks;
		struct iovec *cur_vecs;
		size_t cur_vec_count;
		size_t cur_vec_offset;

		int res;

		size_t bytes_transferred;

		while( vec_offset > 0 && vec_count > 0 ) {
			vec_offset -= vec->len;
			++vec;
			--vec_count;
		}

		if( vec_count == 0 ) {
			res = ERR_INVALID_ARGS;
			goto err2;
		}

		block_pos = pos / block_size;

		if( block_pos > capacity ) {
			res = ERR_INVALID_ARGS;
			goto err2;
		}

		block_ofs = pos - block_pos * block_size;

		need_buffer =
			block_ofs != 0 ||
			len < (ssize_t)block_size ||
			!blkman_check_alignment( vec, vec_count, vec_offset,
				device->params.alignment );

retry:
		if( need_buffer ) {
			mutex_lock( &blkman_buffer_mutex );

			if( write &&
				(block_ofs != 0 || len < (ssize_t)block_size) )
			{
				cur_blocks = 1;

				res = handle->device->interface->read( handle->handle_cookie,
					&blkman_buffer_phys_vec, block_pos,
					cur_blocks, &bytes_transferred );

			} else {
				cur_blocks = (len + block_ofs + block_size - 1) / block_size;
				if( cur_blocks * block_size > blkman_buffer_size )
					cur_blocks = blkman_buffer_size / block_size;
			}

			if( write )
				blkman_copy_buffer( blkman_buffer + block_ofs,
					vec, vec_count, vec_offset, cur_blocks * block_size, true );

			cur_vecs = blkman_buffer_vec;
			cur_vec_count = 1;
			cur_vec_offset = 0;

		} else {
			cur_blocks = len / block_size;

			cur_vecs = vec;
			cur_vec_count = vec_count;
			cur_vec_offset = vec_offset;
		}

		cur_blocks = min( cur_blocks, device->params.max_blocks );

		if( block_pos + cur_blocks > capacity )
			cur_blocks = capacity - block_pos;

		if( cur_blocks > device->params.max_blocks )
			cur_blocks = device->params.max_blocks;

		cur_len = cur_blocks * block_size;

		if( need_locking ) {
			for( ; cur_len > 0;
				cur_blocks >>= 1, cur_len = cur_blocks * block_size )
			{
				size_t locked_len;

				locked_len = blkman_lock_iovecs( cur_vecs, cur_vec_count,
					cur_vec_offset, len,
					write ? VM_LOCK_DMA_FROM_MEMORY : VM_LOCK_DMA_TO_MEMORY );

				if( locked_len == cur_len )
					break;

				if( locked_len > block_size ) {
					cur_blocks = locked_len / block_size;
					cur_len = cur_blocks * block_size;
					break;
				}

				blkman_unlock_iovecs( cur_vecs, cur_vec_count, cur_vec_offset, locked_len );
			}

			if( cur_len == 0 ) {
				if( need_buffer )
					panic( "Cannot lock scratch buffer\n" );

				need_buffer = true;
				goto retry;
			}

		}

		res = blkman_map_iovecs(
			cur_vecs, cur_vec_count, cur_vec_offset, cur_len,
			phys_vecs, device->params.max_sg_num,
			device->params.dma_boundary, device->params.dma_boundary_solid );

		if( res != NO_ERROR )
			goto cannot_map;

		if( phys_vecs->total_len < cur_len ) {
			cur_blocks = phys_vecs->total_len / block_size;

			if( cur_blocks == 0 ) {
				if( need_locking )
					blkman_unlock_iovecs( cur_vecs, cur_vec_count, cur_vec_offset, cur_len );

				if( need_buffer )
					panic( "Scratch buffer turned out to be too fragmented !?\n" );

				need_buffer = true;
				goto retry;
			}

			phys_vecs->total_len = cur_blocks * block_size;
		}

		if( write )
			res = handle->device->interface->write( handle->handle_cookie,
				phys_vecs, block_pos,
				cur_blocks, &bytes_transferred );
		else
			res = handle->device->interface->read( handle->handle_cookie,
				phys_vecs, block_pos,
				cur_blocks, &bytes_transferred );

cannot_map:
		if( need_locking )
			blkman_unlock_iovecs( cur_vecs, cur_vec_count, cur_vec_offset, cur_len );

		if( res != NO_ERROR ) {
			if( need_buffer )
				mutex_unlock( &blkman_buffer_mutex );

			goto err2;
		}

		if( need_buffer ) {
			if( !write ) {
				blkman_copy_buffer( blkman_buffer + block_ofs,
					vec, vec_count, vec_offset, bytes_transferred, false );
			}

			mutex_unlock( &blkman_buffer_mutex );
		}

		len -= bytes_transferred;
		vec_offset += bytes_transferred;
	}

	locked_pool->free( handle->device->phys_vecs_pool, phys_vecs );

	return orig_len;

err2:
	locked_pool->free( handle->device->phys_vecs_pool, phys_vecs );

	return res;

err:
	return res;
}

static ssize_t blkman_readv_int( blkman_handle_info *handle, struct iovec *vec,
	size_t vec_count, off_t pos, ssize_t len, bool need_locking )
{
	return blkman_readwrite( handle, vec, vec_count, pos, len, need_locking, false );
}

static ssize_t blkman_writev_int( blkman_handle_info *handle, struct iovec *vec,
	size_t vec_count, off_t pos, ssize_t len, bool need_locking )
{
	return blkman_readwrite( handle, vec, vec_count, pos, len, need_locking, true );
}

static ssize_t blkman_readpage( blkman_handle_info *handle, iovecs *vecs, off_t pos)
{
	return blkman_readv_int( handle, vecs->vec, vecs->num, pos, vecs->total_len, false );
}

static ssize_t blkman_writepage( blkman_handle_info *handle, iovecs *vecs, off_t pos)
{
	return blkman_writev_int( handle, vecs->vec, vecs->num, pos, vecs->total_len, false );
}

static inline ssize_t blkman_readwritev( blkman_handle_info *handle, struct iovec *vec,
	size_t vec_count, off_t pos, ssize_t len, bool write )
{
	int res;

	if( (res = vm_lock_range( (addr_t)vec, vec_count * sizeof( vec[0] ), 0 )) != NO_ERROR )
		return res;

	if( write )
		res = blkman_writev_int( handle, vec, vec_count, pos, len, true );
	else
		res = blkman_readv_int( handle, vec, vec_count, pos, len, true );

	vm_unlock_range( (addr_t)vec, vec_count * sizeof( vec[0] ));

	return res;
}

static ssize_t blkman_readv( blkman_handle_info *handle, struct iovec *vec,
	size_t vec_count, off_t pos, ssize_t len )
{
	return blkman_readwritev( handle, vec, vec_count, pos, len, false );
}

static ssize_t blkman_read( blkman_handle_info *handle, void *buf, off_t pos, ssize_t len )
{
	iovec vec[1];

	vec[0].start = buf;
	vec[0].len = len;

	return blkman_readv( handle, vec, 1, pos, len );
}

static ssize_t blkman_writev( blkman_handle_info *handle, struct iovec *vec,
	size_t vec_count, off_t pos, ssize_t len )
{
	return blkman_readwritev( handle, vec, vec_count, pos, len, true );
}

static ssize_t blkman_write( blkman_handle_info *handle, void *buf, off_t pos, ssize_t len )
{
	iovec vec[1];

	vec[0].start = buf;
	vec[0].len = len;

	return blkman_writev( handle, vec, 1, pos, len );
}

static int blkman_canpage( blkman_handle_info *handle )
{
	return true;
}

/*static phys_vecs *blkman_alloc_phys_vecs( blkman_device_info *device )
{
	phys_vecs *vec;

retry:
	mutex_lock( &device->lock );

	vec = device->free_phys_vecs;
	if( vec == NULL ) {
		mutex_unlock( &device->lock );
		sem_acquire( device->phys_vec_avail, 1 );
		goto retry;
	}

	device->free_phys_vecs = *((phys_vecs **)vec);

	mutex_unlock( &device->lock );

	return vec;
}

static void blkman_free_phys_vecs( blkman_device_info *device, phys_vecs *vec )
{
	bool was_empty;

	mutex_lock( &device->lock );

	was_empty = device->free_phys_vecs == NULL;

	*((phys_vecs **)vec) = device->free_phys_vecs;
	device->free_phys_vecs = vec;

	mutex_unlock( &device->lock );

	if( was_empty )
		sem_release( device->phys_vec_avail, 1 );
}*/

static int blkman_set_capacity( blkman_device_info *device, uint64 capacity,
	size_t block_size )
{
	mutex_lock( &device->lock );
	device->params.capacity = capacity;
	device->params.block_size = block_size;
	mutex_unlock( &device->lock );

	return NO_ERROR;
}

static int blkman_init_buffer( void )
{
	int res;

	SHOW_FLOW0( 3, "" );

	blkman_buffer_size = 32*1024;

	res = mutex_init( &blkman_buffer_mutex, "blkman_buffer_mutex" );
	if( res < 0 )
		goto err1;

	res = blkman_buffer_region = vm_create_anonymous_region( vm_get_kernel_aspace_id(),
		"blkman_buffer", (void **)&blkman_buffer, REGION_ADDR_ANY_ADDRESS,
		blkman_buffer_size,
		REGION_WIRING_WIRED, LOCK_RW | LOCK_KERNEL );

	if( res < 0 )
		goto err2;

	res = vm_get_page_mapping( vm_get_kernel_aspace_id(), (addr_t)blkman_buffer,
		&blkman_buffer_phys );

	if( res < 0 )
		goto err3;

	blkman_buffer_vec[0].start = blkman_buffer;
	blkman_buffer_vec[0].len = blkman_buffer_size;

	blkman_buffer_phys_vec.num = 1;
	blkman_buffer_phys_vec.total_len = blkman_buffer_size;
	blkman_buffer_phys_vec.vec[0].start = blkman_buffer_phys;
	blkman_buffer_phys_vec.vec[0].len = blkman_buffer_size;

	return NO_ERROR;

err3:
	vm_delete_region( vm_get_kernel_aspace_id(), blkman_buffer_region );
err2:
	mutex_destroy( &blkman_buffer_mutex );
err1:
	return res;
}

static int blkman_uninit_buffer( void )
{
	vm_delete_region( vm_get_kernel_aspace_id(), blkman_buffer_region );
	mutex_destroy( &blkman_buffer_mutex );

	return NO_ERROR;
}

static int blkman_init( void )
{
	int res;

	SHOW_FLOW0( 3, "" );

	/*res = module_get( PARTITIONS_MANAGER_MODULE_NAME, 0, (void **)&part_mngr );
	if( res != NO_ERROR )
		goto err1;*/

	res = module_get( LOCKED_POOL_MODULE_NAME, 0, (void **)&locked_pool );
	if( res != NO_ERROR )
		goto err2;

	res = blkman_init_buffer();
	if( res != NO_ERROR )
		goto err3;

	return NO_ERROR;

err3:
	module_put( LOCKED_POOL_MODULE_NAME );
err2:
	/*module_put( PARTITIONS_MANAGER_MODULE_NAME );*/
//err1:
	return res;
}

static int blkman_uninit( void )
{
	SHOW_FLOW0( 3, "" );

	blkman_uninit_buffer();
	module_put( LOCKED_POOL_MODULE_NAME );
	/*module_put( PARTITIONS_MANAGER_MODULE_NAME );*/

	return NO_ERROR;
}


struct dev_calls dev_interface = {
	(int (*)(dev_ident, dev_cookie *cookie))	blkman_open,
	(int (*)(dev_cookie))						blkman_close,
	(int (*)(dev_cookie))						blkman_freecookie,

	(int (*)(dev_cookie, off_t, seek_type))		blkman_seek,
	(int (*)(dev_cookie, int, void *, size_t))	blkman_ioctl,

	(ssize_t (*)(dev_cookie, void *, off_t, ssize_t))		blkman_read,
	(ssize_t (*)(dev_cookie, const void *, off_t, ssize_t))	blkman_write,

	(int (*)(dev_ident))						blkman_canpage,
	(ssize_t (*)(dev_ident, iovecs *, off_t))	blkman_readpage,
	(ssize_t (*)(dev_ident, iovecs *, off_t))	blkman_writepage,
};


blkman_interface blkman = {
	blkman_register_dev,
	blkman_unregister_dev,
	blkman_set_capacity
};

module_header blkman_module = {
	BLKMAN_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,

	&blkman,

	blkman_init,
	blkman_uninit
};

module_header *modules[] =
{
	&blkman_module,
	NULL
};


// map->num contains maximum number of entries on call
static int vm_get_page_range_mapping( vm_address_space *aspace, addr_t vaddr, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len )
{
	size_t cur_idx = 0;
	size_t left_len = len;

	for( left_len = len, cur_idx = 0;
		left_len > 0 && cur_idx < max_entries; )
	{
		int res;

		if( (res = vm_get_page_mapping( vm_get_current_user_aspace_id(),
			vaddr, &map[cur_idx].start )) != NO_ERROR )
			return res;

		map[cur_idx].len = min( ROUND_PAGE_UP( vaddr ) - vaddr, left_len );
		left_len -= map[cur_idx].len;
		vaddr += map[cur_idx].len;

		if( cur_idx > 0 &&
			map[cur_idx].start == map[cur_idx - 1].start + map[cur_idx - 1].len )
		{
			map[cur_idx - 1].len += map[cur_idx].len;
		} else {
			++cur_idx;
		}
	}

	*num_entries = len - left_len;
	*mapped_len = cur_idx;

	return NO_ERROR;
}

int vm_get_iovec_page_mapping(
	iovec *vec, size_t vec_count, size_t vec_offset, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len )
{
	size_t cur_idx;
	size_t left_len;

	while( vec_count > 0 && vec_offset > vec->len ) {
		vec_offset -= vec->len;
		--vec_count;
		++vec;
	}

	for( left_len = len, cur_idx = 0;
		left_len > 0 && vec_count > 0 && cur_idx < max_entries;
		++vec, --vec_count )
	{
		addr_t range_start;
		size_t range_len;
		int res;
		size_t cur_num_entries, cur_mapped_len;

		range_start = (addr_t)vec->start + vec_offset;
		range_len = min( vec->len - vec_offset, left_len );

		vec_offset = 0;

		if( (res = vm_get_page_range_mapping( vm_get_current_user_aspace(),
			range_start, range_len, &map[cur_idx], max_entries - cur_idx,
			&cur_num_entries, &cur_mapped_len ))
			!= NO_ERROR )
			return res;

		if( cur_num_entries > 0 && cur_idx > 0 &&
			map[cur_idx].start == map[cur_idx - 1].start + map[cur_idx - 1].len )
		{
			map[cur_idx - 1].len = map[cur_idx].len;
			memcpy( &map[cur_idx], &map[cur_idx + 1],
				(cur_num_entries - 1) * sizeof( phys_vec ));
		}

		cur_idx += cur_num_entries;
		left_len -= cur_mapped_len;
	}

	*num_entries = cur_idx;
	*mapped_len = len - left_len;

	return NO_ERROR;
}

int vm_lock_range( addr_t vaddr, size_t len, int flags )
{
	return NO_ERROR;
}

int vm_unlock_range( addr_t vaddr, size_t len )
{
	return NO_ERROR;
}
/*
void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest;
	char *s = (char *)src;

	while(count--)
		*tmp++ = *s++;

	return dest;
}
*/
int devfs_unpublish_device( const char *name )
{
	return ERR_UNIMPLEMENTED;
}
