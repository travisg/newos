/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/sem.h>
#include <kernel/heap.h>
#include <kernel/debug.h>

#include <kernel/arch/cpu.h>

#include <boot/stage2.h>

#include <libc/string.h>

// heap stuff
// ripped mostly from nujeffos

struct heap_page {
	unsigned short bin_index : 5;
	unsigned short free_count : 9;
	unsigned short cleaning : 1;
	unsigned short in_use : 1;
} PACKED;

static struct heap_page *heap_alloc_table; 
static unsigned int heap_base;
static unsigned int heap_size;

struct heap_bin {
	unsigned int element_size;
	unsigned int grow_size;
	unsigned int alloc_count;
	void *free_list;
	unsigned int free_count;
	char *raw_list;
	unsigned int raw_count;
};
static struct heap_bin bins[] = {
	{16, PAGE_SIZE, 0, 0, 0, 0, 0},
	{32, PAGE_SIZE, 0, 0, 0, 0, 0},
	{44, PAGE_SIZE, 0, 0, 0, 0, 0},
	{64, PAGE_SIZE, 0, 0, 0, 0, 0},
	{89, PAGE_SIZE, 0, 0, 0, 0, 0},
	{128, PAGE_SIZE, 0, 0, 0, 0, 0},
	{256, PAGE_SIZE, 0, 0, 0, 0, 0},
	{315, PAGE_SIZE, 0, 0, 0, 0, 0},
	{512, PAGE_SIZE, 0, 0, 0, 0, 0},
	{1024, PAGE_SIZE, 0, 0, 0, 0, 0},
	{2048, PAGE_SIZE, 0, 0, 0, 0, 0},
	{0x1000, 0x1000, 0, 0, 0, 0, 0},
	{0x2000, 0x2000, 0, 0, 0, 0, 0},
	{0x3000, 0x3000, 0, 0, 0, 0, 0},
	{0x4000, 0x4000, 0, 0, 0, 0, 0},
	{0x5000, 0x5000, 0, 0, 0, 0, 0},
	{0x6000, 0x6000, 0, 0, 0, 0, 0},
	{0x7000, 0x7000, 0, 0, 0, 0, 0},
	{0x8000, 0x8000, 0, 0, 0, 0, 0},
	{0x9000, 0x9000, 0, 0, 0, 0, 0},
	{0xa000, 0xa000, 0, 0, 0, 0, 0},
	{0xb000, 0xb000, 0, 0, 0, 0, 0},
	{0xc000, 0xc000, 0, 0, 0, 0, 0},
	{0xd000, 0xd000, 0, 0, 0, 0, 0},
	{0xe000, 0xe000, 0, 0, 0, 0, 0},
	{0xf000, 0xf000, 0, 0, 0, 0, 0},
	{0x10000, 0x10000, 0, 0, 0, 0, 0} // 64k
};

static const int bin_count = sizeof(bins) / sizeof(struct heap_bin);
static sem_id heap_sem = -1;

// called from vm_init. The heap should already be mapped in at this point, we just
// do a little housekeeping to set up the data structure.
int heap_init(addr new_heap_base, unsigned int new_heap_size)
{
	// set some global pointers
	heap_alloc_table = (struct heap_page *)new_heap_base;
	heap_size = new_heap_size;
	heap_base = PAGE_ALIGN((unsigned int)heap_alloc_table + (heap_size / PAGE_SIZE) * sizeof(struct heap_page));
	dprintf("heap_alloc_table = 0x%x, heap_base = 0x%x\n", heap_alloc_table, heap_base);
	
	// zero out the heap alloc table at the base of the heap
	memset((void *)heap_alloc_table, 0, (heap_size / PAGE_SIZE) * sizeof(struct heap_page));
	
	return 0;
}

int heap_init_postsem(kernel_args *ka)
{
	heap_sem = sem_create(1, "heap_sem");
	if(heap_sem < 0) {
		panic("error creating heap semaphore\n");
	}
	return 0;
}

static char *raw_alloc(unsigned int size, int bin_index)
{
	unsigned int new_heap_ptr;
	char *retval;
	struct heap_page *page;
	unsigned int addr;
	
	new_heap_ptr = heap_base + PAGE_ALIGN(size);
	if(new_heap_ptr > heap_base + heap_size) {
		dprintf("heap overgrew itself! spinning forever...\n");
		for(;;);
	}

	for(addr = heap_base; addr < heap_base + size; addr += PAGE_SIZE) {
		page = &heap_alloc_table[(addr - heap_base) / PAGE_SIZE];
		page->in_use = 1;
		page->cleaning = 0;
		page->bin_index = bin_index;
		if (bin_index < bin_count && bins[bin_index].element_size < PAGE_SIZE)
			page->free_count = PAGE_SIZE / bins[bin_index].element_size;
		else
			page->free_count = 1;
	}
	
	retval = (char *)heap_base;
	heap_base = new_heap_ptr;
	return retval;
}

void *kmalloc(unsigned int size)
{
	void *address = NULL;
	int bin_index;
	unsigned int i;
	struct heap_page *page;

	sem_acquire(heap_sem, 1);
	
	for (bin_index = 0; bin_index < bin_count; bin_index++)
		if (size <= bins[bin_index].element_size)
			break;

	if (bin_index == bin_count) {
		// XXX fix the raw alloc later.
		//address = raw_alloc(size, bin_index);
		goto out;
	} else {
		if (bins[bin_index].free_list != NULL) {
			address = bins[bin_index].free_list;
			bins[bin_index].free_list = (void *)(*(unsigned int *)bins[bin_index].free_list);
			bins[bin_index].free_count--;
		} else {
			if (bins[bin_index].raw_count == 0) {
				bins[bin_index].raw_list = raw_alloc(bins[bin_index].grow_size, bin_index);
				bins[bin_index].raw_count = bins[bin_index].grow_size / bins[bin_index].element_size;
			}

			bins[bin_index].raw_count--;
			address = bins[bin_index].raw_list;
			bins[bin_index].raw_list += bins[bin_index].element_size;
		}
		
		bins[bin_index].alloc_count++;
		page = &heap_alloc_table[((unsigned int)address - heap_base) / PAGE_SIZE];
		for(i = 0; i < bins[bin_index].element_size / PAGE_SIZE; i++)
			page[i].free_count--;
	}

out:
	sem_release(heap_sem, 1);
	
//	dprintf("kmalloc: asked to allocate size %d, returning ptr = %p\n", size, address);

	return address;
}

void kfree(void *address)
{
	struct heap_page *page;
	struct heap_bin *bin;
	unsigned int i;

	if (address == NULL)
		return;

	sem_acquire(heap_sem, 1);

//	dprintf("kfree: asked to free at ptr = %p\n", address);

	page = &heap_alloc_table[((unsigned)address - heap_base) / PAGE_SIZE];
	
	// XXX fix later
	if(page[0].bin_index == bin_count)
		goto out;

	bin = &bins[page[0].bin_index];

	for(i = 0; i < bin->element_size / PAGE_SIZE; i++)
		page[i].free_count++;

	*(unsigned int *)address = (unsigned int)bin->free_list;
	bin->free_list = address;
	bin->alloc_count--;
	bin->free_count++;

out:
	sem_release(heap_sem, 1);
}
