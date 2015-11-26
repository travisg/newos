/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __LOCKED_POOL_H__
#define __LOCKED_POOL_H__

typedef struct locked_pool *locked_pool_cookie;

typedef int (*locked_pool_alloc_hook)( void *block, void *arg );
typedef void (*locked_pool_free_hook)( void *block, void *arg );

typedef struct {
    void *(*alloc)( locked_pool_cookie pool );
    void (*free)( locked_pool_cookie pool, void *block );

    locked_pool_cookie (*init)(
        int block_size, int alignment, int next_ofs,
        int chunk_size, int max_blocks, int min_free_blocks,
        const char *name, int wiring_flags,
        locked_pool_alloc_hook alloc_hook,
        locked_pool_free_hook free_hook,
        void *hook_arg );
    void (*uninit)( locked_pool_cookie pool );
} locked_pool_interface;

#define LOCKED_POOL_MODULE_NAME "generic/locked_pool/v1"

#endif
