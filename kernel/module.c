/*
** Copyright 2001, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/module.h>
#include <kernel/lock.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>

#include <kernel/elf.h>

#include <nulibc/string.h>

typedef struct module_info {
	struct module_info *next, *prev;
	char *image_name;
	module_header *header;
	int ref_count;
	bool ready;
} module_info;


static module_info module_list = {
	&module_list, &module_list,
	NULL,
	NULL,
	-1,
	false
};

// XXX locking scheme: there is a global lock only; having several locks
// makes trouble if dependent modules get loaded concurrently ->
// they have to wait for each other, i.e. we need one lock per module;
// also we should detect circular references during init and not dead-lock
static recursive_lock modules_lock;		

module_info *find_module( const char *name )
{
	module_info *module;
	
	recursive_lock_lock( &modules_lock );
	
	for( module = module_list.next; module != &module_list; module = module->next ) {
		if( strcmp( module->header->name, name ) == 0 ) {
			atomic_add( &module->ref_count, 1 );
			return module;
		}
	}
	
	return NULL;
}

module_info *add_module( module_header *header, const char *file_name )
{
	module_info *module;
	
	module = (module_info *)kmalloc( sizeof( module_info ));
	
	if( module == NULL )
		return module;
		
	module->header = header;
	module->image_name = kstrdup( strlen( file_name ));
	module->ref_count = 1;
	module->ready = false;
	
	module->prev = &module_list;
	module->next = module_list.next;
	module_list.next->prev = module;
	module_list.next = module;
	
	return module;
}

void remove_module( module_info *module )
{
	module->prev->next = module->next;
	module->next->prev = module->prev;

	kfree( module->image_name );	
	kfree( module );
}

// load module "name" with base path "prefix"
// "prefix" must *not* have trailing slash
int load_module( const char *name, const char *prefix, module_info **module )
{
	char full_name[SYS_MAX_PATH_LEN];
	image_id image;
	module_header **local_modules, **header;
	module_info *tmp_module;
	int pos;
	int res = ERR_NOT_FOUND;
	
	strlcpy( full_name, prefix, sizeof(full_name) );
	
	pos = 0;

	// the path of the module can be either physical or logical,
	// e.g. a module called "where/are/you/v1" can be stored in the 
	// file "where", "where/are", "where/are/you" or "where/are/you/v1"
	do {
		if( *name == 0 )
			goto err;
			
		if( pos == SYS_MAX_PATH_LEN - 1 )
			goto err;
			
		if( *name == '/' )
			++name;
			
		if( pos < SYS_MAX_PATH_LEN - 1 )
			full_name[pos++] = '/';
		
		while( pos < SYS_MAX_PATH_LEN - 1 && *name != '/' && *name != 0 ) {
			full_name[pos++] = *name++;
		}
		
		full_name[pos] = 0;
		
		image = elf_load_kspace( full_name, "" );	
	} while( image < 0 );
	
	local_modules = (module_header **)elf_lookup_symbol( image, "modules" );

	if( local_modules == NULL ) {
		goto err2;
	}
		
	for( header = local_modules; *header; ++header ) {
		if( strcmp( (*header)->name, name ) == 0 )
			break;
	}
	
	if( !*header ) {
		goto err2;
	}
	
	if( (*header)->version != MODULE_CURR_VERSION ) {
		dprintf( "Module %s has unsupported version (%i)\n", name, (*header)->version );
		goto err2;
	}
	
	if( !(*header)->interface ) {
		dprintf( "Module %s has no public interface\n", name );
		goto err2;
	}

	tmp_module = add_module( *header, full_name );
	
	res = (*header)->init();
	if( res )
		goto err3;
		
	tmp_module->ready = true;
	*module = tmp_module;	

	return NO_ERROR;

err3:
	remove_module( tmp_module );
err2:
	elf_unload_kspace( full_name );
err:
	return res;
}

int unload_module( module_info *module )
{
	int res;
	
	res = module->header->uninit();
	
	if( res != NO_ERROR ) {
		dprintf( "Error unloading module %s (%i)\n", module->header->name, res );
		// XXX let it stick in memory or mark it as broken ?
		//module->ref_count = MAX_INT / 2;
	}
	
	elf_unload_kspace( module->image_name );
	remove_module( module );
	
	return NO_ERROR;
}

int module_get( const char *name, int flags, void **interface )
{
	module_info *module;
	int res;
	
	dprintf( "get_module: %s\n", name );
	
	if( flags != 0 )
		return ERR_INVALID_ARGS;

	module = find_module( name );
	
	if( !module->ready ) {
		dprintf( "Circular module reference to %s\n", name );
		return ERR_GENERAL;
	}
	
	if( !module )
		res = load_module( name, "/home/config/modules", &module );

	if( !module )
		res = load_module( name, "/boot/modules", &module );
		
	if( !module ) 
		return res;
	
	recursive_lock_unlock( &modules_lock );
	
	*interface = module->header->interface;
	return NO_ERROR;
}

int module_put( const char *name )
{
	module_info *module;
	int res;
	
	dprintf( "put_module: %s\n", name );
	
	module = find_module( name );
	if( !module )
		return ERR_NOT_FOUND;
	
	if( atomic_add( &module->ref_count, -1 ) == 0 )
		res = unload_module( module );
	else
		res = NO_ERROR;
		
	recursive_lock_unlock( &modules_lock );
	
	return res;
}

int module_init( kernel_args *ka )
{
	dprintf( "module: init\n" );
	recursive_lock_create( &modules_lock );
	
	return NO_ERROR;
}
