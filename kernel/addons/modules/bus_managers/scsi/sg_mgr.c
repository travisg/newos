/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "sg_mgr.h"

#include <string.h>

locked_pool_cookie temp_sg_pool;

// for real SCSI controllers, there's no limit to transmission length
// but we need a limit - IDE transmits up to 128K, so we allow that
// (for massive data transmission peripheral drivers should provide own
// SG list anyway)
// add one in case data is not page aligned
#define MAX_TEMP_SG_FRAGMENTS (128*1024/PAGE_SIZE + 1)

typedef struct phys_vec {
	addr_t start;					// physical address
	size_t len;
} phys_vec;


int vm_get_iovec_page_mapping( aspace_id aspace,
	iovec *vec, size_t vec_count, size_t vec_offset, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len );

bool create_temp_sg( CCB_SCSIIO *ccb )
{
	struct sg_elem *temp_sg;
	size_t mapped_len;
	int res;
	size_t num_entries;
	iovec vec = {
		ccb->cam_data,
		ccb->cam_dxfer_len
	};

	SHOW_FLOW0( 3, "" );

	ccb->cam_sg_list = temp_sg = locked_pool->alloc( temp_sg_pool );

	res = vm_get_iovec_page_mapping( vm_get_kernel_aspace_id(),
		&vec, 1, 0, ccb->cam_dxfer_len,
		(phys_vec *)temp_sg, MAX_TEMP_SG_FRAGMENTS, &num_entries,
		&mapped_len );

	if( res == NO_ERROR && mapped_len == ccb->cam_dxfer_len ) {
		ccb->cam_sglist_cnt = num_entries;
		return true;
	}

	SHOW_INFO( 1, "cannot create temporary SG list for IO request (%s)",
		strerror( res ));

	// we don't free sg list here, it's done automatically
	// on cleanup of request
	return false;
}

void cleanup_tmp_sg( CCB_SCSIIO *ccb )
{
	locked_pool->free( temp_sg_pool, ccb->cam_sg_list );
}

int init_temp_sg( void )
{
	temp_sg_pool = locked_pool->init( MAX_TEMP_SG_FRAGMENTS * sizeof( struct sg_elem ),
		sizeof( struct sg_elem ) - 1, 0,
		PAGE_SIZE, 4, 1,
		"xpt_temp_sg_pool", REGION_WIRING_WIRED_CONTIG,
		NULL, NULL, NULL );

	if( temp_sg_pool == NULL )
		return ERR_NO_MEMORY;

	return NO_ERROR;
}

void uninit_temp_sg( void )
{
	locked_pool->uninit( temp_sg_pool );
}


#define ROUND_PAGE_DOWN( addr ) ( (addr) & ~(PAGE_SIZE - 1))
#define ROUND_PAGE_UP( addr ) ( ((addr) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

// map->num contains maximum number of entries on call
static int vm_get_page_range_mapping( aspace_id aspace, addr_t vaddr, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len )
{
	size_t cur_idx = 0;
	size_t left_len = len;

	for( left_len = len, cur_idx = 0;
		left_len > 0 && cur_idx < max_entries; )
	{
		int res;

		SHOW_FLOW( 3, "vaddr=%p, left_len=%x", (void *)vaddr, (int)left_len );

		if( (res = vm_get_page_mapping( aspace,
			vaddr, &map[cur_idx].start )) != NO_ERROR )
			return res;

		// TBD:
		// current vm_get_page_mapping dicards lower bits
		// I'm not sure about side-effects if I fix that
		// meanwhile, the following assignment fixes that
		map[cur_idx].start =
			(map[cur_idx].start & ~(PAGE_SIZE - 1)) |
			(vaddr & (PAGE_SIZE - 1));

		map[cur_idx].len = min( ROUND_PAGE_UP( vaddr ) - vaddr, left_len );
		left_len -= map[cur_idx].len;
		vaddr += map[cur_idx].len;

		SHOW_FLOW( 3, "map->start=%p, map->len=%x",
			(void *)map[cur_idx].start, (int)map[cur_idx].len );

		if( cur_idx > 0 &&
			map[cur_idx].start == map[cur_idx - 1].start + map[cur_idx - 1].len )
		{
			SHOW_FLOW0( 3, "combining with previous page" );
			map[cur_idx - 1].len += map[cur_idx].len;
		} else {
			++cur_idx;
		}
	}

	*num_entries = cur_idx;
	*mapped_len = len - left_len;

	SHOW_FLOW( 3, "num_entries=%i, mapped_len=%x",
		(int)*num_entries, (int)*mapped_len );

	return NO_ERROR;
}

int vm_get_iovec_page_mapping( aspace_id aspace,
	iovec *vec, size_t vec_count, size_t vec_offset, size_t len,
	phys_vec *map, size_t max_entries, size_t *num_entries, size_t *mapped_len )
{
	size_t cur_idx;
	size_t left_len;

	SHOW_FLOW( 3, "vec_count=%i, vec_offset=%i, len=%i, max_entries=%i",
		(int)vec_count, (int)vec_offset, (int)len, (int)max_entries );

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

		SHOW_FLOW( 3, "left_len=%i, vec_count=%i, cur_idx=%i",
			(int)left_len, (int)vec_count, (int)cur_idx );

		range_start = (addr_t)vec->start + vec_offset;
		range_len = min( vec->len - vec_offset, left_len );

		SHOW_FLOW( 3, "range_start=%x, range_len=%x",
			(int)range_start, (int)range_len );

		vec_offset = 0;

		if( (res = vm_get_page_range_mapping( aspace,
			range_start, range_len, &map[cur_idx], max_entries - cur_idx,
			&cur_num_entries, &cur_mapped_len ))
			!= NO_ERROR )
		{
			SHOW_ERROR( 1, "invalid io_vec passed (%s)", strerror( res ));
			return res;
		}

		SHOW_FLOW( 3, "cur_num_entries=%i, cur_mapped_len=%x",
			(int)cur_num_entries, (int)cur_mapped_len );

		if( cur_num_entries > 0 && cur_idx > 0 &&
			map[cur_idx].start == map[cur_idx - 1].start + map[cur_idx - 1].len )
		{
			SHOW_FLOW0( 3, "combine with previous chunk" );
			map[cur_idx - 1].len = map[cur_idx].len;
			memcpy( &map[cur_idx], &map[cur_idx + 1],
				(cur_num_entries - 1) * sizeof( phys_vec ));
			--cur_num_entries;
		}

		cur_idx += cur_num_entries;
		left_len -= cur_mapped_len;
	}

	*num_entries = cur_idx;
	*mapped_len = len - left_len;

	SHOW_FLOW( 3, "num_entries=%i, mapped_len=%x",
		(int)*num_entries, (int)*mapped_len );

	return NO_ERROR;
}
