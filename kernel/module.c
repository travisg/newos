/*
** Copyright 2001, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

//
// TODO:
// - all functions in <kernel/khash.h> require non-const keys 
//   -> make them const and remove type-casts here
// - "hash_func" of <kernel/khash.h> must pass "range" unsigned
//   -> change declaration there and fix all applications
// - move string hash function to <kernel/khash.h>
// - "offsetof" macro is missing
//   -> move it to a public place

#include <kernel/module.h>
#include <kernel/lock.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <kernel/khash.h>

#include <kernel/elf.h>

#include <nulibc/string.h>

#if 1
#include <kernel/bus/isa/isa.h>
#endif


#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif

bool modules_disable_user_addons = false;

#define debug_level_flow 0
#define debug_level_error 1
#define debug_level_info 1

#define WAIT 
#define WAIT_ERROR
#define MSG_PREFIX "MODULE -- "

#define FUNC_NAME MSG_PREFIX __FUNCTION__ ": "

#define SHOW_FLOW(seriousness, format, param...) \
	do { if( debug_level_flow > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME, param ); WAIT \
	}} while( 0 )

#define SHOW_FLOW0(seriousness, format) \
	do { if( debug_level_flow > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME); WAIT \
	}} while( 0 )

#define SHOW_ERROR(seriousness, format, param...) \
	do { if( debug_level_error > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME, param ); WAIT_ERROR \
	}} while( 0 )

#define SHOW_ERROR0(seriousness, format) \
	do { if( debug_level_error > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME); WAIT_ERROR \
	}} while( 0 )

#define SHOW_INFO(seriousness, format, param...) \
	do { if( debug_level_info > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME, param ); WAIT \
	}} while( 0 )

#define SHOW_INFO0(seriousness, format) \
	do { if( debug_level_info > seriousness ) { \
		dprintf( "%s"##format, FUNC_NAME); WAIT \
	}} while( 0 )


typedef enum {
	module_loaded,
	module_initializing,
	module_ready,
	module_uninitializing,
	module_err_uninit
} module_state;

typedef struct module_info {
	struct module_info *next;
	struct module_image *image;
	module_header *header;
	int ref_count;
	bool keep_loaded;
	module_state state;
} module_info;


typedef struct module_image {
	struct module_image *next;
	int ref_count;
	bool keep_loaded;
	char *name;
	char *path;
	int base_path_id;
	module_header **headers;
} module_image;


// XXX locking scheme: there is a global lock only; having several locks
// makes trouble if dependent modules get loaded concurrently ->
// they have to wait for each other, i.e. we need one lock per module;
// also we must detect circular references during init and not dead-lock
static recursive_lock modules_lock;		

const char *const module_paths[] = {
	"/boot/user-addons", 
	"/boot/addons"
};

#define num_module_paths (sizeof( module_paths ) / sizeof( module_paths[0] ))

#define MODULES_HASH_SIZE 16

void *modules_images[num_module_paths];
void *modules_list;


static int module_hash_str( const char *str )
{
	char ch;
	unsigned int hash = 0;
	
	// we assume hash to be at least 32 bits
	while( (ch = *str++) != 0 ) {
		hash ^= hash >> 28;
		hash <<= 4;
		hash ^= ch;
	}
	
	return (int)hash;
}

static int module_info_compare( void *a, void *key )
{
	module_info *module = (module_info *)a;
	const char *name = (const char *)key;
	
	return strcmp( module->header->name, name );
}

static unsigned int module_info_hash( void *a, void *key, int range_in )
{
	module_info *module = (module_info *)a;
	const char *name = (const char *)key;
	unsigned int range = (unsigned int)range_in;	

	if( module != NULL )
		return module_hash_str( module->header->name ) % range;
	else
		return module_hash_str( key ) % range;
}

static int module_image_compare( void *a, void *key )
{
	module_image *image = (module_image *)a;
	const char *name = (const char *)key;
	
	return strcmp( image->name, name );
}

static unsigned int module_image_hash( void *a, void *key, int range_in )
{
	module_image *image = (module_image *)a;
	const char *name = (const char *)key;
	unsigned int range = (unsigned int)range_in;	

	if( image != NULL )
		return module_hash_str( image->name ) % range;
	else
		return module_hash_str( key ) % range;
}


static inline int check_module_header( module_header *header )
{
	if( header->version != MODULE_CURR_VERSION ) {
		SHOW_ERROR( 0, "module %s has unsupported version (%i)\n", header->name, header->version );
		return ERR_GENERAL;;
	}
	
	if( header->interface == NULL ) {
		SHOW_ERROR( 0, "module %s has no public interface\n", header->name );
		return ERR_GENERAL;
	}
	
	if( (header->flags & ~(MODULE_KEEP_LOADED)) != 0 ) {
		SHOW_ERROR( 0, "module %s has unknown flag(s) set (%x)\n", header->name, header->flags );
		return ERR_GENERAL;
	}
	
	return NO_ERROR;
}


static inline module_info *register_module( module_image *image, module_header *header )
{
	module_info *module;
	
	SHOW_FLOW( 3, "module %s in image %s\n", header->name, image->path );
	
	if( check_module_header( header ) != NO_ERROR )
		return NULL;
	
	module = (module_info *)kmalloc( sizeof( module_info ));
	
	if( module == NULL )
		return NULL;
		
	module->header = header;
	module->image = image;
	module->ref_count = 1;
	module->keep_loaded = header->flags & MODULE_KEEP_LOADED;
	image->keep_loaded |= module->keep_loaded;
	module->state = module_loaded;
	
	SHOW_FLOW0( 3, "adding to hash\n" );
	if( hash_insert( modules_list, module ) != NO_ERROR ) {
		kfree( module );
		return NULL;
	}
	
	++image->ref_count;
	
	SHOW_FLOW0( 3, "done\n" );
	
	return module;
}

static inline void unregister_module_image( module_image *image )
{
	SHOW_FLOW( 3, "name=%s\n", image->name );
	
	hash_remove( modules_images[image->base_path_id], image );
	
	kfree( image->name );	
	kfree( image->path );
	kfree( image );
}

static void put_module_image( module_image *image )
{
	if( --image->ref_count == 0 ) {
		SHOW_FLOW( 3, "image %s not in use anymore\n", image->name );
		
		if( image->keep_loaded )
			return;
		
		SHOW_FLOW( 1, "unloading image... %s\n", image->path );	
		elf_unload_kspace( image->path );
		SHOW_FLOW0( 1, "...done\n" );
		
		unregister_module_image( image );
	}
}

static inline void unregister_module( module_info *module )
{
	SHOW_FLOW( 3, "%s\n", module->header->name );
	hash_remove( modules_list, module );
	
	put_module_image( module->image );

	kfree( module );
}


static inline module_image *register_module_image_int( const char *image_name, const char *path, 
	int base_path_id, module_header **headers, bool keep_loaded )
{
	module_image *image;
	int res;
	
	image = (module_image *)kmalloc( sizeof( module_image ));
	
	if( image == NULL )
		return NULL;
		
	memset( image, 0, sizeof( image ));
	image->ref_count = 1;
	image->keep_loaded = keep_loaded;
	image->name = kstrdup( image_name );
	if( image->name == NULL )
		goto err;
		
	image->path = kstrdup( path );
	if( image->path == NULL )
		goto err2;

	image->base_path_id = base_path_id;		
	image->headers = headers;

	SHOW_FLOW( 3, "adding image to hash %i\n", base_path_id );	
	if( hash_insert( modules_images[base_path_id], image ) != NO_ERROR )
		goto err3;
	
	return image;
	
err3:
	kfree( image->path );
err2:
	kfree( image->name );
err:
	kfree( image );
	return NULL;
}

static module_image *register_module_image( const char *image_name, const char *path, 
	int base_path_id, module_header **headers, bool keep_loaded )
{
	module_image *image;
	module_header **header;
	
	SHOW_FLOW( 3, "module %s\n", image_name );
	
	image = register_module_image_int( image_name, path, base_path_id, headers, keep_loaded );
	if( image == NULL )
		return NULL;
		
	for( header = image->headers; *header; ++header ) {
		if( strncmp( (*header)->name, image->name, strlen( image->name )) != 0 ) 
			SHOW_INFO( 0, "module %s stored in wrong image (%s)\n", (*header)->name, image->name );
	}
	
	return image;
}

static inline module_image *load_module_image( const char *image_name, const char *path, int base_path_id )
{
	image_id file_image;
	module_header **headers;
	module_image *image;
	
	SHOW_FLOW( 2, "loading %s\n", path );
	
	file_image = elf_load_kspace( path, "" );
	
	if( file_image < 0 ) {
		SHOW_FLOW( 3, "couldn't load image %s (%s)\n", path, strerror( file_image ));
		return NULL;
	}

	headers = (module_header **)elf_lookup_symbol( file_image, "modules" );

	if( headers == NULL ) {
		SHOW_INFO( 0, "image %s has no interface\n", path );
		elf_unload_kspace( path );
		return NULL;
	}
	
	SHOW_FLOW( 1, "loaded image %s\n", image_name );
	
	image = register_module_image( image_name, path, base_path_id, headers, false );
	
	if( image == NULL  ) {
		elf_unload_kspace( path );
		return NULL;
	}
	
	return image;
}

static inline module_header *find_module_in_image( module_image *image, const char *name )
{
	module_header **header;
	
	for( header = image->headers; *header; ++header ) {
		if( strcmp( (*header)->name, name ) == 0 ) {
			SHOW_FLOW( 3, "found module %s in image %s\n", name, image->name );
			return *header;
		}
	}
	
	SHOW_FLOW( 3, "cannot find module %s in image %s\n", name, image->name );
	return NULL;
}

static module_image *get_module_image( const char *name, size_t name_len, int base_path_id )
{
	char path[SYS_MAX_PATH_LEN];
	char *rel_name;
	size_t total_len;
	module_image *image;
	
	if( base_path_id == 0 && modules_disable_user_addons ) {
		SHOW_FLOW0( 3, "ignored - user add-ons are disabled\n" );
		return NULL;
	}

	total_len = min( name_len, SYS_MAX_PATH_LEN ) + 1;
	memcpy( path, name, total_len - 1 );
	path[total_len - 1] = 0;
	
	SHOW_FLOW( 3, "%s\n", path );
	
	image = hash_lookup( modules_images[base_path_id], (void *)path );
	
	if( image ) {
		SHOW_FLOW0( 3, "image already loaded\n" );
		++image->ref_count;
		return image;
	}
		
	total_len = strlen( module_paths[base_path_id] ) + 1 + name_len + 1;
		
	if( total_len > SYS_MAX_PATH_LEN ) {
		SHOW_FLOW0( 3, "ups, path too long\n" );
		return NULL;
	}
		
	strcpy( path, module_paths[base_path_id] );
	strcat( path, "/" );
	rel_name = path + strlen( path );
	
	strncat( path, name, name_len );
	path[total_len - 1] = 0;
	
	SHOW_FLOW( 3, "trying to load %s\n", path );

	image = load_module_image( rel_name, path, base_path_id );
	return image;
}

static module_info *search_module( const char *name )
{
	int pos;
	module_info *module;
	
	SHOW_FLOW( 3, "name: %s\n", name );
	
	module = hash_lookup( modules_list, (void *)name );
		
	if( module != NULL ) {
		SHOW_FLOW0( 3, "already loaded\n" );
		
		++module->ref_count;
		return module;
	}
	
	SHOW_FLOW0( 3, "try to load it\n" );
	pos = strlen( name );
	
	while( pos >= 0 ) {
		int i;
		
		for( i = 0; i < (int)num_module_paths; ++i ) {
			module_image *image;
			
			image = get_module_image( name, pos, i );
			
			if( image ) {
				module_header *header;
				
				SHOW_FLOW0( 3, "got image\n" );
				
				header = find_module_in_image( image, name );
				if( header ) {
					SHOW_FLOW0( 3, "got header\n" );
					
					module = register_module( image, header );
					
					put_module_image( image );
					return module;
				}
					
				put_module_image( image );
			}
		}
		
		--pos;
		
		while( pos > 0 && name[pos] != '/' )
			--pos;
	}
	
	return NULL;
}

static inline int init_module( module_info *module )
{
	int res;
	
	switch( module->state ) {
	case module_loaded:
		module->state = module_initializing;	
		SHOW_FLOW( 3, "initing module %s... \n", module->header->name );
		res = module->header->init();
		SHOW_FLOW( 3, "...done (%s)\n", strerror( res ));
		
		if( !res ) 
			module->state = module_ready;
		else
			module->state = module_loaded;
		break;

	case module_ready:
		res = NO_ERROR;
		break;
		
	case module_initializing:
		SHOW_ERROR( 0, "circular reference to %s\n", module->header->name );
		res = ERR_GENERAL;
		break;
		
	case module_uninitializing:
		SHOW_ERROR( 0, "tried to load module %s which is currently unloading\n", module->header->name );
		res = ERR_GENERAL;
		break;

	case module_err_uninit:
		SHOW_INFO( 0, "cannot load module %s because its earlier unloading failed\n", module->header->name );
		res = ERR_GENERAL;
		break;
		
	default:
		res = ERR_GENERAL;
	}
	
	return res;
}

static inline int uninit_module( module_info *module )
{
	switch( module->state ) {
	case module_loaded:
		return NO_ERROR;

	case module_initializing:
		panic( "Trying to unload module %s which is initializing\n", 
			module->header->name );
		return ERR_GENERAL;

	case module_uninitializing:
		panic( "Trying to unload module %s which is un-initializing\n", module->header->name );
		return ERR_GENERAL;
		
	case module_ready:
		{
			int res;
			
			module->state = module_uninitializing;

			SHOW_FLOW( 2, "uniniting module %s...\n", module->header->name );
			res = module->header->uninit();
			SHOW_FLOW( 2, "...done (%s)\n", strerror( res ));
			
			if( res == NO_ERROR ) {
				module->state = module_loaded;
				return NO_ERROR;
			}
			
			SHOW_ERROR( 0, "Error unloading module %s (%i)\n", module->header->name, res );
		}
		
		module->state = module_err_uninit;
		module->keep_loaded = true;
		module->image->keep_loaded = true;
			
		// fall through
	default:	
		return ERR_GENERAL;		
	}
}

static int put_module_info( module_info *module )
{
	int res;
	
	if( --module->ref_count != 0 ) 
		return NO_ERROR;
		
	SHOW_FLOW( 2, "module %s not in use anymore\n", module->header->name );
	
	if( module->keep_loaded )
		return NO_ERROR;
	
	res = uninit_module( module );
	
	if( res == NO_ERROR )
		unregister_module( module );

	return res;
}

int module_get( const char *name, int flags, void **interface )
{
	module_image *image;
	module_info *module;
	int res;
	int i;
	
	SHOW_FLOW( 0, "name=%s, flags=%i\n", name, flags );
	
	if( flags != 0 )
		return ERR_INVALID_ARGS;
		
	recursive_lock_lock( &modules_lock );

	module = search_module( name );
	
	if( !module ) {
		SHOW_FLOW( 3, "couldn't find module %s\n", name );
		res = ERR_NOT_FOUND;
		goto err;
	}
	
	SHOW_FLOW0( 3, "make sure module is ready\n" );
	res = init_module( module );
		
	if( res != NO_ERROR )
		goto err2;
		
	SHOW_FLOW0( 3, "module's ready to use\n" );
		
	*interface = module->header->interface;
	
	recursive_lock_unlock( &modules_lock );		
	return res;

err2:
	put_module_info( module );
err:	
	recursive_lock_unlock( &modules_lock );
	return res;
}

int module_put( const char *name )
{
	module_image *image;
	module_info *module;
	int res;
	
	SHOW_FLOW( 0, "name=%s\n", name );

	recursive_lock_lock( &modules_lock );

	module = hash_lookup( modules_list, (void *)name );
	if( module == NULL ) {
		res = ERR_NOT_FOUND;
		goto err;
	}

	res = put_module_info( module );

err:
	recursive_lock_unlock( &modules_lock );	
	return res;
}


typedef struct module_iterator {
	char *prefix;
	module_image *cur_image;
	module_header **cur_header;
	int base_path_id;
	struct module_dir_iterator *base_dir, *cur_dir;
	int err;
	int prefix_pos;
	unsigned int prefix_base_path_id;
} module_iterator;

typedef struct module_dir_iterator {
	struct module_dir_iterator *parent_dir, *sub_dir;
	char *name;
	int file;
} module_dir_iterator;


modules_cookie module_open_list( const char *prefix )
{
	module_iterator *iter;
	
	SHOW_FLOW( 3, "prefix: %s\n", prefix );
	
	if( (iter = kmalloc( sizeof( *iter ))) == NULL )
		return NULL;

	iter->prefix = kstrdup( prefix );
	if( iter == NULL ) {
		kfree( iter );
		return NULL;
	}
	
	/*iter->cur_image = module_sys_image;
	++module_sys_image->ref_count;
	iter->cur_header = iter->cur_image->headers;*/
	iter->cur_image = NULL;
	iter->cur_header = NULL;
	
	iter->base_path_id = -1;
	iter->base_dir = iter->cur_dir = NULL;
	iter->err = NO_ERROR;
	iter->prefix_pos = strlen( prefix );
	
	iter->prefix_base_path_id = 0;
	
	return (modules_cookie)iter;
}


static inline int module_enter_image( module_iterator *iter, const char *image_name )
{
	module_image *image;
	module_info *module;
	
	SHOW_FLOW( 3, "%s\n", image_name );
	
	image = get_module_image( image_name, strlen( image_name ), iter->base_path_id );
		
	if( image == NULL ) {
		SHOW_FLOW( 3, "cannot get image %s\n", image_name );
			
		return NO_ERROR;
	} 
		
	iter->cur_image = image;
	iter->cur_header = image->headers;
	
	SHOW_FLOW( 3, "entered image %s\n", image_name );
	return NO_ERROR;
}

static inline int module_create_dir_iterator( module_iterator *iter, int file, const char *name )
{
	module_dir_iterator *dir;
	
	dir = kmalloc( sizeof( *dir ));
	if( dir == NULL )
		return ERR_NO_MEMORY;
		
	dir->name = kstrdup( name );
	if( dir->name == NULL ) {
		kfree( dir );
		return ERR_NO_MEMORY;
	}

	dir->file = file;

	dir->sub_dir = NULL;		
	dir->parent_dir = iter->cur_dir;
	
	if( iter->cur_dir )
		iter->cur_dir->sub_dir = dir;
	else
		iter->base_dir = dir;
		
	iter->cur_dir = dir;

	SHOW_FLOW( 3, "created dir iterator for %s\n", name );		
	return NO_ERROR;
}

static inline int module_enter_dir( module_iterator *iter, const char *path, const char *name  )
{
	int file;
	int res;
	
	file = sys_open( path, STREAM_TYPE_DIR, 0 );
	if( file < 0 ) {
		SHOW_FLOW( 3, "couldn't open directory %s (%s)\n", path, strerror( file ));
		
		// there are so many errors for "not found" that we don't bother
		// and always assume that the directory suddenly disappeared
		return NO_ERROR;
	}
						
	res = module_create_dir_iterator( iter, file, name );
	if( res != NO_ERROR ) {
		sys_close( file );
		return ERR_NO_MEMORY;
	}
	
	SHOW_FLOW( 3, "entered directory %s\n", path );				
	return NO_ERROR;
}


static inline void destroy_dir_iterator( module_iterator *iter )
{
	module_dir_iterator *dir;
	
	dir = iter->cur_dir;
	
	SHOW_FLOW( 3, "destroying directory iterator for sub-dir %s\n", dir->name );
	
	if( dir->parent_dir )
		dir->parent_dir->sub_dir = NULL;
		
	iter->cur_dir = dir->parent_dir;
		
	kfree( dir->name );
	kfree( dir );
}


static inline void module_leave_dir( module_iterator *iter )
{
	module_dir_iterator *parent_dir;
	
	SHOW_FLOW( 3, "leaving directory %s\n", iter->cur_dir->name );
	
	parent_dir = iter->cur_dir->parent_dir;
	
	sys_close( iter->cur_dir->file );
	destroy_dir_iterator( iter );
	
	iter->cur_dir = parent_dir;
}

static void compose_path( char *path, module_iterator *iter, const char *name, bool full_path )
{
	module_dir_iterator *dir;
	
	if( full_path ) {
		strlcpy( path, iter->base_dir->name, SYS_MAX_PATH_LEN );
		strlcat( path, "/", SYS_MAX_PATH_LEN );
	} else {
		strlcpy( path, iter->prefix, SYS_MAX_PATH_LEN );
		if( *iter->prefix )
			strlcat( path, "/", SYS_MAX_PATH_LEN );
	}
	
	for( dir = iter->base_dir->sub_dir; dir; dir = dir->sub_dir ) {
		strlcat( path, dir->name, SYS_MAX_PATH_LEN );
		strlcat( path, "/", SYS_MAX_PATH_LEN );
	}
		
	strlcat( path, name, SYS_MAX_PATH_LEN );
	
	SHOW_FLOW( 3, "name: %s, %s -> %s\n", name, 
		full_path ? "full path" : "relative path", 
		path );
}


static inline int module_traverse_dir( module_iterator *iter )
{
	int res;
	struct file_stat stat;
	char name[SYS_MAX_NAME_LEN];
	char path[SYS_MAX_PATH_LEN];
	
	SHOW_FLOW( 3, "scanning %s\n", iter->cur_dir->name );
	
	if( (res = sys_read( iter->cur_dir->file, name, 0, sizeof( name ))) <= 0 ) {
		SHOW_FLOW( 3, "got error: %s\n", strerror( res ));
		module_leave_dir( iter );
		return NO_ERROR;
	}
	
	SHOW_FLOW( 3, "got %s\n", name );
	
	if( strcmp( name, "." ) == 0 ||
		strcmp( name, ".." ) == 0 )
		return NO_ERROR;
		
	// currently, sys_read returns an error if buffer is too small
	// I don't know the official specification, so it's always safe
	// to add a trailing end-of-string
	name[sizeof(name) - 1] = 0;
	
	compose_path( path, iter, name, true );
	
	if( (res = sys_rstat( path, &stat)) != NO_ERROR )
		return res;
		
	switch( stat.type ) {
		case STREAM_TYPE_FILE:
			compose_path( path, iter, name, false );

			return module_enter_image( iter, path );

		case STREAM_TYPE_DIR:
			return module_enter_dir( iter, path, name );

		default:
			SHOW_FLOW( 3, "entry %s not a file nor a directory - ignored\n", name );
			return NO_ERROR;
	}
}


static inline int module_enter_master_image( module_iterator *iter )
{
	module_image *image;
	
	SHOW_FLOW0( 3, "\n" );
	
	if( iter->prefix_base_path_id >= num_module_paths ) {
		
		SHOW_FLOW0( 3, "reducing image name\n" );
		--iter->prefix_pos;
		
		while( 
			iter->prefix_pos > 0 && 
			iter->prefix[iter->prefix_pos] != '/' )
		{
			--iter->prefix_pos;
		}
		
		iter->prefix_base_path_id = 0;
		
		if( iter->prefix_pos < 0 ) {
			SHOW_FLOW0( 3, "no possible master image left\n" );
			return NO_ERROR;
		}
	}
	
	SHOW_FLOW0( 3, "trying new image\n" );
	image = get_module_image( iter->prefix, iter->prefix_pos, iter->prefix_base_path_id );
	
	++iter->prefix_base_path_id;
	
	if( image ) {
		SHOW_FLOW( 3, "got image %s\n", image->name );
		
		iter->cur_image = image;
		iter->cur_header = image->headers;
	}
	
	return NO_ERROR;
}

static inline int module_enter_base_path( module_iterator *iter )
{
	char path[SYS_MAX_PATH_LEN];
	struct file_stat stat;
	int res;
	
	++iter->base_path_id;
	
	if( iter->base_path_id >= (int)num_module_paths ) {
		SHOW_FLOW0( 3, "no locations left\n" );
		return ERR_NOT_FOUND;
	}

	SHOW_FLOW( 3, "checking new base path (%s)\n", module_paths[iter->base_path_id] );
	
	if( iter->base_path_id == 0 && modules_disable_user_addons ) {
		SHOW_FLOW0( 3, "ignoring user add-ons (they are disabled)\n" );
		return NO_ERROR;
	}
		
	strcpy( path, module_paths[iter->base_path_id] );
	if( *iter->prefix ) {
		strcat( path, "/" );
		strlcat( path, iter->prefix, sizeof( path ));
	}

	res = module_enter_dir( iter, path, path );
	
	return res;		
}

static inline bool iter_check_module_header( module_iterator *iter )
{
	module_info *module;
	module_image *tmp_image;
	const char *name;
	
	if( *iter->cur_header == NULL ) {
		SHOW_FLOW0( 3, "reached end of modules list in image\n" );
	
		put_module_image( iter->cur_image );
	
		iter->cur_header = NULL;
		iter->cur_image = NULL;
		
		return false;
	}
	
	name = (*iter->cur_header)->name;
	
	SHOW_FLOW( 3, "checking %s\n", name );

	if( strlen( name ) < strlen( iter->prefix ) ||
		memcmp( name, iter->prefix, strlen( iter->prefix )) != 0 )
	{
		SHOW_FLOW( 3, "module %s has wrong prefix\n", name );
		
		++iter->cur_header;
		return false;
	}
	
	module = search_module( name );
	
	if( module == NULL ) {
		SHOW_FLOW( 3, "couldn't get module %s -> must be broken\n", name );
		
		++iter->cur_header;
		return false;
	}
	
	SHOW_FLOW0( 3, "get rid of image\n" );
	
	tmp_image = module->image;
	++tmp_image->ref_count;
	
	put_module_info( module );
	
	if( iter->base_path_id >= 0 ) {
		if( strlen( tmp_image->name ) < strlen( iter->prefix ) ||
			tmp_image->base_path_id != iter->base_path_id ) 
		{
			if( strlen( tmp_image->name ) < strlen( iter->prefix ))
				SHOW_FLOW( 3, "image name (%s) is shorter then prefix -> already scanned\n", 
					tmp_image->name );
			else
				SHOW_FLOW( 3, "image (%s) is from wrong base path\n", tmp_image->name );

			put_module_image( tmp_image );
			++iter->cur_header;
			return false;
		}
	}
		
	put_module_image( tmp_image );
	
	SHOW_FLOW( 3, "found module %s\n", name );
	return true;
}

int read_next_module_name( modules_cookie cookie, char *buf, size_t *bufsize )
{
	module_iterator *iter = (module_iterator *)cookie;
	int res;
	
	if( !iter )
		return ERR_INVALID_ARGS;
		
	res = iter->err;
		
	while( res == NO_ERROR ) {
		if( iter->cur_header ) {
			SHOW_FLOW0( 3, "having image to scan\n" );

			if( iter_check_module_header( iter )) {
				strlcpy( buf, (*iter->cur_header)->name, *bufsize );
				*bufsize = strlen( buf );
		
				++iter->cur_header;

				res = NO_ERROR;
				break;
			}
		} else {
			SHOW_FLOW0( 3, "looking for new image\n" );
			
			if( iter->cur_dir == NULL ) {
				if( iter->prefix_pos >= 0 ) 
					res = module_enter_master_image( iter );
				else
					res = module_enter_base_path( iter );
			} else
				res = module_traverse_dir( iter );
		}
	}
	
	iter->err = res;
	
	SHOW_FLOW( 3, "finished with status %s\n", strerror( iter->err ));
	return iter->err;
}


int close_module_list( modules_cookie cookie )
{
	module_iterator *iter = (module_iterator *)cookie;
	
	SHOW_FLOW0( 3, "\n" );
	
	if( !iter )
		return ERR_INVALID_ARGS;
		
	while( iter->cur_dir )
		module_leave_dir( iter );

	kfree( iter->prefix );
	kfree( iter );
	return NO_ERROR;
}

module_header *dummy[] = { NULL };


int module_init( kernel_args *ka, module_header **sys_module_headers )
{
	int res;
	unsigned int i;
		
	SHOW_FLOW0( 0, "\n" );
	recursive_lock_create( &modules_lock );
	
	modules_list = hash_init( MODULES_HASH_SIZE, 
		offsetof( module_info, next ),
		module_info_compare, module_info_hash );
		
	if( modules_list == NULL )
		return ERR_NO_MEMORY;
		
	for( i = 0; i < num_module_paths; ++i ) {
		modules_images[i] = hash_init( MODULES_HASH_SIZE, 
			offsetof( module_image, next ),
			module_image_compare, module_image_hash);
			
		if( modules_images[i] == NULL )
			return ERR_NO_MEMORY;
	}

	if( sys_module_headers == NULL ) 
		sys_module_headers = dummy;
		
	if( register_module_image( "", "(built-in)", 0, sys_module_headers, true ) == NULL )
		return ERR_NO_MEMORY;
		
	#if 1
	{
		isa_bus_manager *isa_interface;
		int res;
		int i;
		void *iterator;
		char module_name[SYS_MAX_PATH_LEN];
		size_t name_len;
		const char prefix[] = "bus_managers";
		modules_cookie modules_cookie;
		
		if( (res = module_get( ISA_MODULE_NAME, 0, (void **)&isa_interface )) != NO_ERROR ) 
			dprintf( "Cannot load isa module (%s)\n", strerror( res ));
		else {
			for( i = 'A'; i <= 'Z'; ++i )
				isa_interface->write_io_8( 0xe9, i );
				
			module_put( ISA_MODULE_NAME );
		}
		
		dprintf( "Getting modules with prefix %s\n", prefix );
		modules_cookie = module_open_list( prefix );

		while( name_len = sizeof( module_name ),
			read_next_module_name( modules_cookie, module_name, &name_len ) == NO_ERROR )
		{
			dprintf( "Found module %s\n", module_name );
		}

		close_module_list( modules_cookie );		
		dprintf( "done\n" );
	}
	#endif
	
	return NO_ERROR;
}
