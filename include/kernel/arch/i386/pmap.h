/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_PMAP
#define _I386_PMAP

int pmap_init_and_add_pgdir_to_list(addr pgdir);
int pmap_remove_pgdir_from_list(addr pgdir);

#endif

