/*
** Copyright 2001-2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

#define MODULE_CURR_VERSION 0x10000
// if set, module gets neither uninit'd nor unloaded
#define MODULE_KEEP_LOADED 1

// user function list and management function lists are separated
struct module_header {
    const char *name;               // with path (for clients convinience)
    uint32 version;
    uint32 flags;
    void *interface;                // pointer to function list

    int ( *init )( void );
    int ( *uninit )( void );
};

typedef struct modules_cookie {} *modules_cookie;

typedef struct module_header module_header;

// to become a module, export this symbol
// it has to be a null-terminated list of pointers to module headers
extern module_header *modules[];

// flags must be 0
extern int module_get( const char *name, int flags, void **interface );
extern int module_put( const char *name );

extern modules_cookie module_open_list( const char *prefix );
extern int read_next_module_name( modules_cookie cookie, char *buf, size_t *bufsize );
extern int close_module_list( modules_cookie cookie );


// boot init
// XXX this is very private, so move it away
extern int module_init( kernel_args *ka, module_header **sys_module_headers );

#endif
