/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/vm_store_anonymous_noswap.h>
#include <sys/errors.h>

static void anonymous_destroy(struct vm_store *store)
{
	if(store) {
		kfree(store);
	}
}

static off_t anonymous_commit(struct vm_store *store, off_t size)
{
	// XXX finish
	return 0;
}

static int anonymous_has_page(struct vm_store *store, off_t offset)
{
	return 0;
}

static int anonymous_read(struct vm_store *store, off_t offset, void *buf, size_t *len)
{
	panic("anonymous_store: read called. Invalid!\n");
	return ERR_UNIMPLEMENTED;
}

static int anonymous_write(struct vm_store *store, off_t offset, const void *buf, size_t *len)
{
	panic("anonymous_store: write called. Invalid!\n");
	return ERR_UNIMPLEMENTED;
}

/*
static int anonymous_fault(struct vm_store *backing_store, struct vm_address_space *aspace, off_t offset)
{
	// unused
}
*/

static vm_store_ops anonymous_ops = {
	&anonymous_destroy,
	&anonymous_commit,
	&anonymous_has_page,
	&anonymous_read,
	&anonymous_write,
	NULL // fault() is unused
};

vm_store *vm_store_create_anonymous_noswap()
{
	vm_store *store;
	
	store = kmalloc(sizeof(vm_store));
	if(store == NULL)
		return NULL;

	store->ops = &anonymous_ops;
	store->cache = NULL;
	store->data = NULL;

	return store;
}

