/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/sem.h>
#include <kernel/arch/cpu.h>
#include <kernel/time.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/vm_cache.h>
#include <kernel/vm_page.h>

bool trimming_cycle;
static addr_t free_memory_low_water;
static addr_t free_memory_high_water;

static void scan_pages(vm_address_space *aspace, addr_t free_target)
{
	vm_region *first_region;
	vm_region *region;
	vm_page *page;
	addr_t va;
	addr_t pa;
	unsigned int flags, flags2;
	int quantum = PAGE_SCAN_QUANTUM;
	int regions_examined = 0;
	int pages_examined = 0;
	int pages_present = 0;
	int pages_unmapped = 0;

	bigtime_t start_time = system_time();

	dprintf("  scan_pages called on aspace %p, id 0x%x, free_target %ld\n", aspace, aspace->id, free_target);

	sem_acquire(aspace->virtual_map.sem, READ_COUNT);

	first_region = aspace->virtual_map.region_list;
	while(first_region && (first_region->base + (first_region->size - 1)) < aspace->scan_va) {
		VERIFY_VM_REGION(first_region);
		first_region = first_region->aspace_next;
	}
	if(first_region)
		VERIFY_VM_REGION(first_region);

	if(!first_region) {
		first_region = aspace->virtual_map.region_list;
	}
	if(first_region)
		VERIFY_VM_REGION(first_region);

	if(!first_region) {
		sem_release(aspace->virtual_map.sem, READ_COUNT);
		return;
	}

	region = first_region;

	for(;;) {
		VERIFY_VM_REGION(region);
		VERIFY_VM_CACHE_REF(region->cache_ref);
		VERIFY_VM_CACHE(region->cache_ref->cache);

		// scan the pages in this region
		mutex_lock(&region->cache_ref->lock);
		if(!region->cache_ref->cache->scan_skip) {
			regions_examined++;
			for(va = region->base; va < (region->base + region->size); va += PAGE_SIZE) {
				pages_examined++;
				aspace->translation_map.ops->query(&aspace->translation_map, va, &pa, &flags);
				if((flags & PAGE_PRESENT) == 0) {
					continue;
				}

				pages_present++;

				page = vm_lookup_page(pa / PAGE_SIZE);
				if(!page) {
					continue;
				}
				VERIFY_VM_PAGE(page);

				// see if this page is busy, if it is lets forget it and move on
				if(page->state == PAGE_STATE_BUSY || page->state == PAGE_STATE_WIRED) {
					continue;
				}

//				dprintf("**va 0x%x pa 0x%x fl 0x%x, st %d\n", va, pa, flags, page->state);

				flags2 = 0;
				if(free_target > 0) {
					// look for a page we can steal
					if(!(flags & PAGE_ACCESSED) && page->state == PAGE_STATE_ACTIVE) {
						pages_unmapped++;

						aspace->translation_map.ops->lock(&aspace->translation_map);

						// unmap the page
						aspace->translation_map.ops->unmap(&aspace->translation_map, va, va + PAGE_SIZE);

						// flush the tlbs of all cpus
						aspace->translation_map.ops->flush(&aspace->translation_map);

						// re-query the flags on the old pte, to make sure we have accurate modified bit data
						aspace->translation_map.ops->query(&aspace->translation_map, va, &pa, &flags2);

						// clear the modified and accessed bits on the entries
						aspace->translation_map.ops->clear_flags(&aspace->translation_map, va, PAGE_MODIFIED|PAGE_ACCESSED);

						// decrement the ref count on the page. If we just unmapped it for the last time,
						// put the page on the inactive list
						if(atomic_add(&page->ref_count, -1) == 1) {
							vm_page_set_state(page, PAGE_STATE_INACTIVE);
							free_target--;
						}
						aspace->translation_map.ops->unlock(&aspace->translation_map);
					} else if(flags & PAGE_ACCESSED) {
						// clear the accessed bits of this page
						aspace->translation_map.ops->lock(&aspace->translation_map);
						aspace->translation_map.ops->clear_flags(&aspace->translation_map, va, PAGE_ACCESSED);
						aspace->translation_map.ops->unlock(&aspace->translation_map);
					}
				}

				// if the page is modified, but the state is active or inactive, put it on the modified list
				if(((flags & PAGE_MODIFIED) || (flags2 & PAGE_MODIFIED))
					&& (page->state == PAGE_STATE_ACTIVE || page->state == PAGE_STATE_INACTIVE)) {
					if(page->cache_ref->cache->temporary)
						vm_page_set_state(page, PAGE_STATE_MODIFIED_TEMPORARY);
					else
						vm_page_set_state(page, PAGE_STATE_MODIFIED);
				}

				if(--quantum == 0)
					break;
			}
		}
		mutex_unlock(&region->cache_ref->lock);
		// move to the next region, wrapping around and stopping if we get back to the first region
		region = region->aspace_next ? region->aspace_next : aspace->virtual_map.region_list;
		if(region == first_region)
			break;

		if(quantum == 0)
			break;
	}

	aspace->scan_va = region ? (first_region->base + first_region->size) : aspace->virtual_map.base;
	sem_release(aspace->virtual_map.sem, READ_COUNT);

	dprintf("  exiting scan_pages, took %Ld usecs (re %d pe %d pp %d pu %d)\n", system_time() - start_time,
		regions_examined, pages_examined, pages_present, pages_unmapped);
}

static int page_daemon()
{
	struct hash_iterator i;
	vm_address_space *old_aspace;
	vm_address_space *aspace;
	addr_t mapped_size;
	addr_t free_memory_target;
	int faults_per_second;
	bigtime_t now;

	dprintf("page daemon starting\n");

	for(;;) {
		thread_snooze(PAGE_DAEMON_INTERVAL);

		// scan through all of the address spaces
		vm_aspace_walk_start(&i);
		aspace = vm_aspace_walk_next(&i);
		while(aspace) {
			VERIFY_VM_ASPACE(aspace);

			mapped_size = aspace->translation_map.ops->get_mapped_size(&aspace->translation_map);

			dprintf("page_daemon: looking at aspace %p, id 0x%x, mapped size %ld\n", aspace, aspace->id, mapped_size);

			now = system_time();
			if(now - aspace->last_working_set_adjust > WORKING_SET_ADJUST_INTERVAL) {
				faults_per_second = (aspace->fault_count * 1000000) / (now - aspace->last_working_set_adjust);
				dprintf("  faults_per_second = %d\n", faults_per_second);
				aspace->last_working_set_adjust = now;
				aspace->fault_count = 0;

				if(faults_per_second > MAX_FAULTS_PER_SECOND
					&& mapped_size >= aspace->working_set_size
					&& aspace->working_set_size < aspace->max_working_set) {

					aspace->working_set_size += WORKING_SET_INCREMENT;
					dprintf("  new working set size = %ld\n", aspace->working_set_size);
				} else if(faults_per_second < MIN_FAULTS_PER_SECOND
					&& mapped_size <= aspace->working_set_size
					&& aspace->working_set_size > aspace->min_working_set) {

					aspace->working_set_size -= WORKING_SET_DECREMENT;
					dprintf("  new working set size = %ld\n", aspace->working_set_size);
				}
			}

			// decide if we need to enter or leave the trimming cycle
			if(!trimming_cycle && vm_page_num_free_pages() < free_memory_low_water)
				trimming_cycle = true;
			else if(trimming_cycle && vm_page_num_free_pages() > free_memory_high_water)
				trimming_cycle = false;

			// scan some pages, trying to free some if needed
			free_memory_target = 0;
			if(trimming_cycle && mapped_size > aspace->working_set_size)
				free_memory_target = mapped_size - aspace->working_set_size;

			scan_pages(aspace, free_memory_target);
//			scan_pages(aspace, 0x7fffffff);

			// must hold a ref to the old aspace while we grab the next one,
			// otherwise the iterator becomes out of date.
			old_aspace = aspace;
			aspace = vm_aspace_walk_next(&i);
			vm_put_aspace(old_aspace);
		}
	}
}

int vm_daemon_init()
{
	thread_id tid;

	trimming_cycle = false;

	// calculate the free memory low and high water at which point we enter/leave trimming phase
	free_memory_low_water = vm_page_num_pages() / 8;
	free_memory_high_water = vm_page_num_pages() / 4;

	// create a kernel thread to select pages for pageout
	tid = thread_create_kernel_thread("page daemon", &page_daemon, NULL);
	thread_set_priority(tid, THREAD_MIN_RT_PRIORITY);
	thread_resume_thread(tid);

	return 0;
}

