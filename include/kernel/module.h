/*
** Copyright 2001-2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

#define MODULE_CURR_VERSION 0x10000

// user function list and management function lists are separated
struct module_header {
	const char *name;				// with path (for clients convinience)
	uint32 version;
	void *interface;				// pointer to function list
	
	int ( *init )( void );
	int ( *uninit )( void );
};

typedef struct module_header module_header;

// to become a modules, export this symbol
// it has to be a null-terminated list of pointers to module headers
extern module_header *modules[];

// flags must be 0
extern int module_get( const char *name, int flags, void **interface );
extern int module_put( const char *name );

// boot init
// XXX this is very private, so move it away
extern int module_init(kernel_args *ka);


#endif
