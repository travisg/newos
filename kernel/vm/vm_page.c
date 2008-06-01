/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/cpu.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/vm_page.h>
#include <kernel/vm_cache.h>
#include <kernel/arch/vm_translation_map.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/int.h>
#include <kernel/thread.h>
#include <kernel/smp.h>
#include <kernel/sem.h>
#include <kernel/list.h>
#include <newos/errors.h>
#include <boot/stage2.h>

#include <string.h>

typedef struct page_queue {
	struct list_node list;
	int count;
} page_queue;

extern bool trimming_cycle;

static page_queue page_free_queue;
static page_queue page_clear_queue;
static page_queue page_active_queue;
static page_queue page_modified_queue;
static page_queue page_modified_temporary_queue;

static vm_page *all_pages;
static addr_t physical_page_offset;
static unsigned int num_pages;

static spinlock_t page_lock;

static sem_id modified_pages_available;

void dump_page_stats(int argc, char **argv);
void dump_free_page_table(int argc, char **argv);
static int vm_page_set_state_nolock(vm_page *page, int page_state);
static void clear_page(addr_t pa);
static int page_scrubber(void *);

static vm_page *dequeue_page(page_queue *q)
{
	vm_page *page;

	page = list_remove_head_type(&q->list, vm_page, queue_node);
	if(page)
		q->count--;

#if DEBUG > 1
	if(page)
		VERIFY_VM_PAGE(page);
#endif

	return page;
}

static void enqueue_page(page_queue *q, vm_page *page)
{
#if DEBUG > 1
	VERIFY_VM_PAGE(page);
#endif
	list_add_head(&q->list, &page->queue_node);
	q->count++;
	if(q == &page_modified_queue || q == &page_modified_temporary_queue) {
		if(q->count == 1)
			sem_release_etc(modified_pages_available, 1, SEM_FLAG_NO_RESCHED);
	}
}

static void remove_page_from_queue(page_queue *q, vm_page *page)
{
#if DEBUG > 1
	VERIFY_VM_PAGE(page);
#endif
	list_delete(&page->queue_node);
	q->count--;
}

static void move_page_to_queue(page_queue *from_q, page_queue *to_q, vm_page *page)
{
#if DEBUG > 1
	VERIFY_VM_PAGE(page);
#endif
	if(from_q != to_q) {
		remove_page_from_queue(from_q, page);
		enqueue_page(to_q, page);
	}
}

static int pageout_daemon()
{
	vm_page *page;
	vm_region *region;
	IOVECS(vecs, 1);
	ssize_t err;

	dprintf("pageout daemon starting\n");

	// XXX remove
	dprintf("pageout daemon disabled, exiting...\n");
	return -1;

	for(;;) {
		sem_acquire(modified_pages_available, 1);

		dprintf("here\n");

		int_disable_interrupts();
		acquire_spinlock(&page_lock);
		page = dequeue_page(&page_modified_queue);
		page->state = PAGE_STATE_BUSY;
		vm_info.modified_pages--;
		vm_info.busy_pages++;
		vm_cache_acquire_ref(page->cache_ref, true);
		release_spinlock(&page_lock);
		int_restore_interrupts();

		dprintf("got page %p\n", page);

		if(page->cache_ref->cache->temporary && !trimming_cycle) {
			// unless we're in the trimming cycle, dont write out pages
			// that back anonymous stores
			int_disable_interrupts();
			acquire_spinlock(&page_lock);
			enqueue_page(&page_modified_queue, page);
			page->state = PAGE_STATE_MODIFIED;
			vm_info.busy_pages--;
			vm_info.modified_pages++;
			release_spinlock(&page_lock);
			int_restore_interrupts();
			vm_cache_release_ref(page->cache_ref);
			continue;
		}

		/* clear the modified flag on this page in all it's mappings */
		mutex_lock(&page->cache_ref->lock);
		list_for_every_entry(&page->cache_ref->region_list_head, region, vm_region, cache_node) {
			if(page->offset > region->cache_offset
			  && page->offset < region->cache_offset + region->size) {
				vm_translation_map *map = &region->aspace->translation_map;
				map->ops->lock(map);
				map->ops->clear_flags(map, page->offset - region->cache_offset + region->base, PAGE_MODIFIED);
				map->ops->unlock(map);
			}
		}
		mutex_unlock(&page->cache_ref->lock);

		/* write the page out to it's backing store */
		vecs->num = 1;
		vecs->total_len = PAGE_SIZE;
		vm_get_physical_page(page->ppn * PAGE_SIZE, (addr_t *)&vecs->vec[0].start, PHYSICAL_PAGE_CAN_WAIT);
		vecs->vec[0].len = PAGE_SIZE;

		err = page->cache_ref->cache->store->ops->write(page->cache_ref->cache->store, page->offset, vecs);

		vm_put_physical_page((addr_t)vecs->vec[0].start);

		int_disable_interrupts();
		acquire_spinlock(&page_lock);
		vm_info.busy_pages--;
		if(page->ref_count > 0) {
			page->state = PAGE_STATE_ACTIVE;
			vm_info.active_pages++;
		} else {
			page->state = PAGE_STATE_INACTIVE;
			vm_info.inactive_pages++;
		}
		enqueue_page(&page_active_queue, page);
		release_spinlock(&page_lock);
		int_restore_interrupts();

		vm_cache_release_ref(page->cache_ref);
	}
}

int vm_page_init(kernel_args *ka)
{
	unsigned int i;

	dprintf("vm_page_init: entry\n");

	page_lock = 0;

	// initialize queues
	list_initialize(&page_free_queue.list);
	page_free_queue.count = 0;
	list_initialize(&page_clear_queue.list);
	page_clear_queue.count = 0;
	list_initialize(&page_active_queue.list);
	page_active_queue.count = 0;
	list_initialize(&page_modified_queue.list);
	page_modified_queue.count = 0;
	list_initialize(&page_modified_temporary_queue.list);
	page_modified_temporary_queue.count = 0;

	// calculate the size of memory by looking at the phys_mem_range array
	{
		unsigned int last_phys_page = 0;

		dprintf("physical memory ranges:\n");
		dprintf("count %d\n", ka->num_phys_mem_ranges);

		physical_page_offset = ka->phys_mem_range[0].start / PAGE_SIZE;
		for(i=0; i<ka->num_phys_mem_ranges; i++) {
			dprintf("\tbase 0x%08lx size 0x%08lx\n", ka->phys_mem_range[i].start, ka->phys_mem_range[i].size);
			last_phys_page = (ka->phys_mem_range[i].start + ka->phys_mem_range[i].size) / PAGE_SIZE - 1;
		}
		dprintf("first phys page = 0x%lx, last 0x%x\n", physical_page_offset, last_phys_page);
		num_pages = last_phys_page - physical_page_offset + 1;
	}

	// set up the global info structure about physical memory
	vm_info.physical_page_size = PAGE_SIZE;
	vm_info.physical_pages = num_pages;

	dprintf("vm_page_init: exit\n");

	return 0;
}

int vm_page_init_postheap(kernel_args *ka)
{
	unsigned int i;

	// map in the new free page table
	all_pages = (vm_page *)vm_alloc_from_ka_struct(ka, num_pages * sizeof(vm_page), LOCK_KERNEL|LOCK_RW);

	dprintf("vm_page_init_postheap: putting free_page_table @ %p, # ents %d (size 0x%x)\n",
		all_pages, num_pages, (unsigned int)(num_pages * sizeof(vm_page)));

	// initialize the free page table
	for(i=0; i < num_pages; i++) {
		all_pages[i].magic = VM_PAGE_MAGIC;
		all_pages[i].ppn = physical_page_offset + i;
		all_pages[i].type = PAGE_TYPE_PHYSICAL;
		all_pages[i].state = PAGE_STATE_FREE;
		all_pages[i].ref_count = 0;
		vm_info.free_pages++;
		enqueue_page(&page_free_queue, &all_pages[i]);
	}

	// mark some of the page ranges inuse
	for(i = 0; i < ka->num_phys_alloc_ranges; i++) {
		vm_mark_page_range_inuse(ka->phys_alloc_range[i].start / PAGE_SIZE,
			ka->phys_alloc_range[i].size / PAGE_SIZE);
	}

	// set the global max_commit variable
	vm_increase_max_commit(num_pages*PAGE_SIZE);

	dprintf("vm_page_init_postheap: exit\n");

	return 0;
}


int vm_page_init2(kernel_args *ka)
{
	void *null;

	null = all_pages;
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "page_structures", &null, REGION_ADDR_EXACT_ADDRESS,
		PAGE_ALIGN(num_pages * sizeof(vm_page)), REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	dbg_add_command(&dump_page_stats, "page_stats", "Dump statistics about page usage");
	dbg_add_command(&dump_free_page_table, "free_pages", "Dump list of free pages");

	return 0;
}

int vm_page_init_postthread(kernel_args *ka)
{
	thread_id tid;

	// create a kernel thread to clear out pages
	tid = thread_create_kernel_thread("page scrubber", &page_scrubber, NULL);
	thread_set_priority(tid, THREAD_LOWEST_PRIORITY);
	thread_resume_thread(tid);

	modified_pages_available = sem_create(0, "modified_pages_avail_sem");

	// create a kernel thread to schedule modified pages to write
	tid = thread_create_kernel_thread("pageout daemon", &pageout_daemon, NULL);
	thread_set_priority(tid, THREAD_MIN_RT_PRIORITY + 1);
	thread_resume_thread(tid);

	return 0;
}

static int page_scrubber(void *unused)
{
#define SCRUB_SIZE 16
	vm_page *page[SCRUB_SIZE];
	int i;
	int scrub_count;

	(void)(unused);

	dprintf("page_scrubber starting...\n");

	for(;;) {
		thread_snooze(100000); // 100ms

		if(page_free_queue.count > 0) {
			int_disable_interrupts();
			acquire_spinlock(&page_lock);

			for(i=0; i<SCRUB_SIZE; i++) {
				page[i] = dequeue_page(&page_free_queue);
				if(page[i] == NULL)
					break;
				vm_info.free_pages--;
			}

			release_spinlock(&page_lock);
			int_restore_interrupts();

			scrub_count = i;

			for(i=0; i<scrub_count; i++) {
				clear_page(page[i]->ppn * PAGE_SIZE);
			}

			int_disable_interrupts();
			acquire_spinlock(&page_lock);

			for(i=0; i<scrub_count; i++) {
				page[i]->state = PAGE_STATE_CLEAR;
				enqueue_page(&page_clear_queue, page[i]);
				vm_info.clear_pages++;
			}

			release_spinlock(&page_lock);
			int_restore_interrupts();
		}
	}

	return 0;
}

static void clear_page(addr_t pa)
{
	addr_t va;

	vm_get_physical_page(pa, &va, PHYSICAL_PAGE_CAN_WAIT);

	memset((void *)va, 0, PAGE_SIZE);

	vm_put_physical_page(va);
}

int vm_mark_page_inuse(addr_t page)
{
	return vm_mark_page_range_inuse(page, 1);
}

int vm_mark_page_range_inuse(addr_t start_page, addr_t len)
{
	vm_page *page;
	addr_t i;

	dprintf("vm_mark_page_range_inuse: start 0x%lx, len 0x%lx\n", start_page, len);

	if(physical_page_offset > start_page) {
		dprintf("vm_mark_page_range_inuse: start page %ld is before free list\n", start_page);
		return ERR_INVALID_ARGS;
	}
	start_page -= physical_page_offset;
	if(start_page + len >= num_pages) {
		dprintf("vm_mark_page_range_inuse: range would extend past free list\n");
		return ERR_INVALID_ARGS;
	}

	int_disable_interrupts();
	acquire_spinlock(&page_lock);

	for(i = 0; i < len; i++) {
		page = &all_pages[start_page + i];
		switch(page->state) {
			case PAGE_STATE_FREE:
			case PAGE_STATE_CLEAR:
				vm_page_set_state_nolock(page, PAGE_STATE_UNUSED);
				break;
			case PAGE_STATE_WIRED:
				break;
			case PAGE_STATE_ACTIVE:
			case PAGE_STATE_INACTIVE:
			case PAGE_STATE_BUSY:
			case PAGE_STATE_MODIFIED:
			case PAGE_STATE_MODIFIED_TEMPORARY:
			case PAGE_STATE_UNUSED:
			default:
				// uh
				dprintf("vm_mark_page_range_inuse: page 0x%lx in non-free state %d!\n", start_page + i, page->state);
		}
	}

	release_spinlock(&page_lock);
	int_restore_interrupts();

	return i;
}

vm_page *vm_page_allocate_specific_page(addr_t page_num, int page_state)
{
	vm_page *p;
	int old_page_state = PAGE_STATE_BUSY;

	int_disable_interrupts();
	acquire_spinlock(&page_lock);

	p = vm_lookup_page(page_num);
	if(p == NULL)
		goto out;

	switch(p->state) {
		case PAGE_STATE_FREE:
			remove_page_from_queue(&page_free_queue, p);
			vm_info.free_pages--;
			break;
		case PAGE_STATE_CLEAR:
			remove_page_from_queue(&page_clear_queue, p);
			vm_info.clear_pages--;
			break;
		case PAGE_STATE_UNUSED:
			break;
		default:
			// we can't allocate this page
			p = NULL;
	}
	if(p == NULL)
		goto out;

	old_page_state = p->state;
	p->state = PAGE_STATE_BUSY;
	vm_info.busy_pages++;

	if(old_page_state != PAGE_STATE_UNUSED)
		enqueue_page(&page_active_queue, p);

out:
	release_spinlock(&page_lock);
	int_restore_interrupts();

	if(p != NULL && page_state == PAGE_STATE_CLEAR &&
		(old_page_state == PAGE_STATE_FREE || old_page_state == PAGE_STATE_UNUSED)) {

		clear_page(p->ppn * PAGE_SIZE);
	}

	return p;
}

vm_page *vm_page_allocate_page(int page_state)
{
	vm_page *p;
	page_queue *q;
	page_queue *q_other;
	int old_page_state;

	switch(page_state) {
		case PAGE_STATE_FREE:
			q = &page_free_queue;
			q_other = &page_clear_queue;
			break;
		case PAGE_STATE_CLEAR:
			q = &page_clear_queue;
			q_other = &page_free_queue;
			break;
		default:
			return NULL; // invalid
	}

	int_disable_interrupts();
	acquire_spinlock(&page_lock);

	p = dequeue_page(q);
	if(p == NULL) {
		// the clear queue was empty, grab one from the free queue and zero it out
		q = q_other;
		p = dequeue_page(q);
		if(p == NULL) {
			// XXX hmm
			panic("vm_allocate_page: out of memory!\n");
		}
	}

	if(q == &page_free_queue)
		vm_info.free_pages--;
	else
		vm_info.clear_pages--;

	old_page_state = p->state;
	p->state = PAGE_STATE_BUSY;
	vm_info.busy_pages++;

	enqueue_page(&page_active_queue, p);

	release_spinlock(&page_lock);
	int_restore_interrupts();

	if(page_state == PAGE_STATE_CLEAR && old_page_state == PAGE_STATE_FREE) {
		clear_page(p->ppn * PAGE_SIZE);
	}

	if(p)
		VERIFY_VM_PAGE(p);

	return p;
}

vm_page *vm_page_allocate_page_run(int page_state, addr_t len)
{
	unsigned int start;
	unsigned int i;
	vm_page *first_page = NULL;

	start = 0;

	int_disable_interrupts();
	acquire_spinlock(&page_lock);

	for(;;) {
		bool foundit = true;
		if(start + len >= num_pages) {
			break;
		}
		for(i = 0; i < len; i++) {
			if(all_pages[start + i].state != PAGE_STATE_FREE &&
			  all_pages[start + i].state != PAGE_STATE_CLEAR) {
				foundit = false;
				i++;
				break;
			}
		}
		if(foundit) {
			// pull the pages out of the appropriate queues
			for(i = 0; i < len; i++)
				vm_page_set_state_nolock(&all_pages[start + i], PAGE_STATE_BUSY);
			first_page = &all_pages[start];
			break;
		} else {
			start += i;
			if(start >= num_pages) {
				// no more pages to look through
				break;
			}
		}
	}
	release_spinlock(&page_lock);
	int_restore_interrupts();

	return first_page;
}

vm_page *vm_lookup_page(addr_t page_num)
{
	if(page_num < physical_page_offset)
		return NULL;
	page_num -= physical_page_offset;
	if(page_num >= num_pages)
		return NULL;

	VERIFY_VM_PAGE(&all_pages[page_num]);

	return &all_pages[page_num];
}

static int vm_page_set_state_nolock(vm_page *page, int page_state)
{
	page_queue *from_q = NULL;
	page_queue *to_q = NULL;

	switch(page->state) {
		case PAGE_STATE_BUSY:
			vm_info.busy_pages--;
			goto removefromactive;
		case PAGE_STATE_ACTIVE:
			vm_info.active_pages--;
			goto removefromactive;
		case PAGE_STATE_INACTIVE:
			vm_info.inactive_pages--;
			goto removefromactive;
		case PAGE_STATE_WIRED:
			vm_info.wired_pages--;
			goto removefromactive;
		case PAGE_STATE_UNUSED:
			vm_info.unused_pages--;
removefromactive:
			from_q = &page_active_queue;
			break;
		case PAGE_STATE_MODIFIED:
			vm_info.modified_pages--;
			from_q = &page_modified_queue;
			break;
		case PAGE_STATE_MODIFIED_TEMPORARY:
			vm_info.modified_temporary_pages--;
			from_q = &page_modified_temporary_queue;
			break;
		case PAGE_STATE_FREE:
			vm_info.free_pages--;
			from_q = &page_free_queue;
			break;
		case PAGE_STATE_CLEAR:
			vm_info.clear_pages--;
			from_q = &page_clear_queue;
			break;
		default:
			panic("vm_page_set_state: vm_page %p in invalid state %d\n", page, page->state);
	}

	switch(page_state) {
		case PAGE_STATE_BUSY:
			vm_info.busy_pages++;
			goto addtoactive;
		case PAGE_STATE_ACTIVE:
			vm_info.active_pages++;
			goto addtoactive;
		case PAGE_STATE_INACTIVE:
			vm_info.inactive_pages++;
			goto addtoactive;
		case PAGE_STATE_WIRED:
			vm_info.wired_pages++;
			goto addtoactive;
		case PAGE_STATE_UNUSED:
			vm_info.unused_pages++;
addtoactive:
			to_q = &page_active_queue;
			break;
		case PAGE_STATE_MODIFIED:
			vm_info.modified_pages++;
			to_q = &page_modified_queue;
			break;
		case PAGE_STATE_MODIFIED_TEMPORARY:
			vm_info.modified_temporary_pages++;
			to_q = &page_modified_temporary_queue;
			break;
		case PAGE_STATE_FREE:
			vm_info.free_pages++;
			to_q = &page_free_queue;
			break;
		case PAGE_STATE_CLEAR:
			vm_info.clear_pages++;
			to_q = &page_clear_queue;
			break;
		default:
			panic("vm_page_set_state: invalid target state %d\n", page_state);
	}
	move_page_to_queue(from_q, to_q, page);
	page->state = page_state;

	return 0;
}

int vm_page_set_state(vm_page *page, int page_state)
{
	int err;

	VERIFY_VM_PAGE(page);

	int_disable_interrupts();
	acquire_spinlock(&page_lock);

	err = vm_page_set_state_nolock(page, page_state);

	release_spinlock(&page_lock);
	int_restore_interrupts();

	return err;
}

addr_t vm_page_num_pages()
{
	return num_pages;
}

addr_t vm_page_num_free_pages()
{
	return page_free_queue.count + page_clear_queue.count;
}

void dump_free_page_table(int argc, char **argv)
{
	// XXX finish
	dprintf("not finished\n");
}

void dump_page_stats(int argc, char **argv)
{
	unsigned int page_types[9];
	addr_t i;

	memset(page_types, 0, sizeof(page_types));

	for(i=0; i<num_pages; i++) {
		page_types[all_pages[i].state]++;
	}

	dprintf("page stats:\n");
	dprintf("active: %d\ninactive: %d\nbusy: %d\nunused: %d\n",
		page_types[PAGE_STATE_ACTIVE], page_types[PAGE_STATE_INACTIVE], page_types[PAGE_STATE_BUSY], page_types[PAGE_STATE_UNUSED]);
	dprintf("modified: %d\nmodified_temporary %d\nfree: %d\nclear: %d\nwired: %d\n",
		page_types[PAGE_STATE_MODIFIED], page_types[PAGE_STATE_MODIFIED_TEMPORARY], page_types[PAGE_STATE_FREE], page_types[PAGE_STATE_CLEAR], page_types[PAGE_STATE_WIRED]);
}


#if 0
static void dump_free_page_table(int argc, char **argv)
{
	unsigned int i = 0;
	unsigned int free_start = END_OF_LIST;
	unsigned int inuse_start = PAGE_INUSE;

	dprintf("dump_free_page_table():\n");
	dprintf("first_free_page_index = %d\n", first_free_page_index);

	while(i < free_page_table_size) {
		if(free_page_table[i] == PAGE_INUSE) {
			if(inuse_start != PAGE_INUSE) {
				i++;
				continue;
			}
			if(free_start != END_OF_LIST) {
				dprintf("free from %d -> %d\n", free_start + free_page_table_base, i-1 + free_page_table_base);
				free_start = END_OF_LIST;
			}
			inuse_start = i;
		} else {
			if(free_start != END_OF_LIST) {
				i++;
				continue;
			}
			if(inuse_start != PAGE_INUSE) {
				dprintf("inuse from %d -> %d\n", inuse_start + free_page_table_base, i-1 + free_page_table_base);
				inuse_start = PAGE_INUSE;
			}
			free_start = i;
		}
		i++;
	}
	if(inuse_start != PAGE_INUSE) {
		dprintf("inuse from %d -> %d\n", inuse_start + free_page_table_base, i-1 + free_page_table_base);
	}
	if(free_start != END_OF_LIST) {
		dprintf("free from %d -> %d\n", free_start + free_page_table_base, i-1 + free_page_table_base);
	}
/*
	for(i=0; i<free_page_table_size; i++) {
		dprintf("%d->%d ", i, free_page_table[i]);
	}
*/
}
#endif
static addr_t vm_alloc_vspace_from_ka_struct(kernel_args *ka, unsigned int size)
{
	addr_t spot = 0;
	unsigned int i;
	int last_valloc_entry = 0;

	size = PAGE_ALIGN(size);
	// find a slot in the virtual allocation addr_t range
	for(i=1; i<ka->num_virt_alloc_ranges; i++) {
		last_valloc_entry = i;
		// check to see if the space between this one and the last is big enough
		if(ka->virt_alloc_range[i].start -
			(ka->virt_alloc_range[i-1].start + ka->virt_alloc_range[i-1].size) >= size) {

			spot = ka->virt_alloc_range[i-1].start + ka->virt_alloc_range[i-1].size;
			ka->virt_alloc_range[i-1].size += size;
			goto out;
		}
	}
	if(spot == 0) {
		// we hadn't found one between allocation ranges. this is ok.
		// see if there's a gap after the last one
		if(ka->virt_alloc_range[last_valloc_entry].start + ka->virt_alloc_range[last_valloc_entry].size + size <=
			KERNEL_BASE + (KERNEL_SIZE - 1)) {
			spot = ka->virt_alloc_range[last_valloc_entry].start + ka->virt_alloc_range[last_valloc_entry].size;
			ka->virt_alloc_range[last_valloc_entry].size += size;
			goto out;
		}
		// see if there's a gap before the first one
		if(ka->virt_alloc_range[0].start > KERNEL_BASE) {
			if(ka->virt_alloc_range[0].start - KERNEL_BASE >= size) {
				ka->virt_alloc_range[0].start -= size;
				spot = ka->virt_alloc_range[0].start;
				goto out;
			}
		}
	}

out:
	return spot;
}

// XXX horrible brute-force method of determining if the page can be allocated
static bool is_page_in_phys_range(kernel_args *ka, addr_t paddr)
{
	unsigned int i;

	for(i=0; i<ka->num_phys_mem_ranges; i++) {
		if(paddr >= ka->phys_mem_range[i].start &&
			paddr < ka->phys_mem_range[i].start + ka->phys_mem_range[i].size) {
			return true;
		}
	}
	return false;
}

addr_t vm_alloc_ppage_from_kernel_struct(kernel_args *ka)
{
	unsigned int i;

	for(i=0; i<ka->num_phys_alloc_ranges; i++) {
		addr_t next_page;

		next_page = ka->phys_alloc_range[i].start + ka->phys_alloc_range[i].size;
		// see if the page after the next allocated paddr run can be allocated
		if(i + 1 < ka->num_phys_alloc_ranges && ka->phys_alloc_range[i+1].size != 0) {
			// see if the next page will collide with the next allocated range
			if(next_page >= ka->phys_alloc_range[i+1].start)
				continue;
		}
		// see if the next physical page fits in the memory block
		if(is_page_in_phys_range(ka, next_page)) {
			// we got one!
			ka->phys_alloc_range[i].size += PAGE_SIZE;
			return ((ka->phys_alloc_range[i].start + ka->phys_alloc_range[i].size - PAGE_SIZE) / PAGE_SIZE);
		}
	}

	return 0;	// could not allocate a block
}

addr_t vm_alloc_from_ka_struct(kernel_args *ka, unsigned int size, int lock)
{
	addr_t vspot;
	addr_t pspot;
	unsigned int i;

	// find the vaddr to allocate at
	vspot = vm_alloc_vspace_from_ka_struct(ka, size);
//	dprintf("alloc_from_ka_struct: vaddr 0x%x\n", vspot);

	// map the pages
	for(i=0; i<PAGE_ALIGN(size)/PAGE_SIZE; i++) {
		pspot = vm_alloc_ppage_from_kernel_struct(ka);
//		dprintf("alloc_from_ka_struct: paddr 0x%x\n", pspot);
		if(pspot == 0)
			panic("error allocating page from ka_struct!\n");
		vm_translation_map_quick_map(ka, vspot + i*PAGE_SIZE, pspot * PAGE_SIZE, lock, &vm_alloc_ppage_from_kernel_struct);
//		pmap_map_page(pspot, vspot + i*PAGE_SIZE, lock);
	}

	return vspot;
}
