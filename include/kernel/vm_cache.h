/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VM_CACHE_H
#define _VM_CACHE_H

#include <kernel/kernel.h>
#include <kernel/vm.h>

vm_cache *vm_cache_create(vm_store *store);
vm_cache_ref *vm_cache_ref_create(vm_cache *cache);
void vm_cache_acquire_ref(vm_cache_ref *cache_ref);
void vm_cache_release_ref(vm_cache_ref *cache_ref);
vm_page *vm_cache_lookup_page(vm_cache_ref *cache_ref, off_t page);
void vm_cache_insert_page(vm_cache_ref *cache_ref, vm_page *page, off_t offset);
void vm_cache_remove_page(vm_cache_ref *cache_ref, vm_page *page);
int vm_cache_insert_region(vm_cache_ref *cache_ref, vm_region *region);
int vm_cache_remove_region(vm_cache_ref *cache_ref, vm_region *region);

#endif

