/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/vm_translation_map.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/vm.h>
#include <kernel/vm_page.h>
#include <kernel/vm_priv.h>
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>
#include <libc/string.h>
#include <boot/stage2.h>

#include <arch/sh4/sh4.h>
#include <arch/sh4/vcpu.h>

typedef struct vm_translation_map_arch_info_struct {
	addr pgdir_virt;
	addr pgdir_phys;
	bool is_user;
} vm_translation_map_arch_info;

static vcpu_struct *vcpu;

#define CHATTY_TMAP 1

#define PHYS_TO_P1(x) ((x) + P1_AREA)

static void invl_page(addr vaddr)
{
	// XXX implement
}

static void destroy_tmap(vm_translation_map *map)
{
	int state;
	vm_translation_map *entry;
	vm_translation_map *last = NULL;

	if(map == NULL)
		return;

	if(map->arch_data->pgdir_virt != NULL)
		kfree((void *)map->arch_data->pgdir_virt);

	kfree(map->arch_data);
}

static int lock_tmap(vm_translation_map *map)
{
	acquire_spinlock(&map->lock);	

	return -1;
}

static int unlock_tmap(vm_translation_map *map)
{
	release_spinlock(&map->lock);

	return -1;
}

static int map_tmap(vm_translation_map *map, addr va, addr pa, unsigned int lock)
{
	struct pdent *pd = NULL;
	struct ptent *pt;
	unsigned int index;

#if CHATTY_TMAP
	dprintf("map_tmap: va 0x%x pa 0x%x lock 0x%x\n", va, pa, lock);
#endif

	if(map->arch_data->is_user) {
		if(va >= P1_AREA) {
			// invalid
			panic("map_tmap: asked to map va 0x%x in user space aspace\n", va);
		}
		pd = (struct pdent *)map->arch_data->pgdir_phys;
		index = va >> 22;
	} else {
		if(va < P3_AREA && va >= P4_AREA) {
			panic("map_tmap: asked to map va 0x%x in kernel space aspace\n", va);
		}
		pd = (struct pdent *)vcpu->kernel_pgdir;
		index = (va & 0x7fffffff) >> 22;
	}

	if(pd[index].v == 0) {
		vm_page *page;
		unsigned int pgtable;

		page = vm_page_allocate_page(PAGE_STATE_CLEAR);
		pgtable = page->ppn * PAGE_SIZE;
		
		// XXX remove when real clear pages support is there
		memset((void *)PHYS_TO_P1(pgtable), 0, PAGE_SIZE);
		
		pd[index].ppn = pgtable >> 12;
		pd[index].v = 1;
	}

	// get the pagetable
	pt = (struct ptent *)PHYS_TO_P1(pd[index].ppn << 12);
	index = (va >> 12) & 0x000003ff;

	// insert the mapping
	pt[index].wt = 0;
	pt[index].pr = (lock & LOCK_KERNEL) ? (lock & 0x1) : (lock | 0x2);
	pt[index].ppn = pa >> 12;
	pt[index].tlb_ent = 0;
	pt[index].c = 1;
	pt[index].sz = 0x1; // 4k page
	pt[index].sh = 0;
	pt[index].d = 0;
	pt[index].v = 1;
		
	invl_page(va);

	return 0;
}

static int unmap_tmap(vm_translation_map *map, addr start, addr end)
{
	struct pdent *pd;
	struct ptent *pt;
	unsigned int index;

	if(end - start > PAGE_SIZE)
		panic("unmap_tmap: asked to unmap more than one page, not implemented\n");
	
	if(map->arch_data->is_user) {
		if(start >= P1_AREA) {
			// invalid
			panic("map_tmap: asked to map va 0x%x in user space aspace\n", start);
		}
		pd = (struct pdent *)map->arch_data->pgdir_phys;
		index = start >> 22;
	} else {
		if(start < P3_AREA && start >= P4_AREA) {
			panic("map_tmap: asked to map va 0x%x in kernel space aspace\n", start);
		}
		pd = (struct pdent *)vcpu->kernel_pgdir;
		index = (start & 0x7fffffff) >> 22;
	}

	if(pd[index].v == 0)
		return -1;
	
	// get the pagetable
	pt = (struct ptent *)PHYS_TO_P1(pd[index].ppn << 12);
	index = (start >> 12) & 0x000003ff;

	if(pt[index].v == 0)
		return -1;

	pt[index].v = 0;

	invl_page(start);
	
	return 0;

}

// XXX currently assumes this translation map is active
static int query_tmap(vm_translation_map *map, addr va, addr *out_physical, unsigned int *out_flags)
{
	struct pdent *pd;
	struct ptent *pt;
	unsigned int index;

	if(map->arch_data->is_user) {
		if(va >= P1_AREA) {
			// invalid
			return -1;
		}
		pd = (struct pdent *)map->arch_data->pgdir_phys;
		index = va >> 22;
	} else {
		if(va < P3_AREA && va >= P4_AREA) {
			return -1;
		}
		pd = (struct pdent *)vcpu->kernel_pgdir;
		index = (va & 0x7fffffff) >> 22;
	}
	
	if(pd[index].v == 0) {
		return -1;
	}
	
	// get the pagetable
	pt = (struct ptent *)PHYS_TO_P1(pd[index].ppn << 12);
	index = (va >> 12) & 0x000003ff;
	
	if(pt[index].v == 0) {
		return -1;
	}
	*out_physical = pt[index].ppn  << 12;	
	
	// XXX fill this
	*out_flags = 0;

	return 0;	
}

static addr get_mapped_size_tmap(vm_translation_map *map)
{
	return map->map_count;
}

static int protect_tmap(vm_translation_map *map, addr base, addr top, unsigned int attributes)
{
	// XXX finish
	return -1;
}

static vm_translation_map_ops tmap_ops = {
	destroy_tmap,
	lock_tmap,
	unlock_tmap,
	map_tmap,
	unmap_tmap,
	query_tmap,
	get_mapped_size_tmap,
	protect_tmap
};

int vm_translation_map_create(vm_translation_map *new_map, bool kernel)
{
	if(new_map == NULL)
		return -1;
	
	// initialize the new object
	new_map->ops = &tmap_ops;
	new_map->lock = 0;
	new_map->map_count = 0;

	new_map->arch_data = kmalloc(sizeof(vm_translation_map_arch_info));
	if(new_map == NULL)
		panic("error allocating translation map object!\n");

	if(!kernel) {
		// user
		// allocate a pgdir
		new_map->arch_data->pgdir_virt = (addr)kmalloc(PAGE_SIZE);
		if(new_map->arch_data->pgdir_virt == NULL) {
			kfree(new_map->arch_data);
			return -1;
		}
		if(((addr)new_map->arch_data->pgdir_virt % PAGE_SIZE) != 0)
			panic("vm_translation_map_create: malloced pgdir and found it wasn't aligned!\n");
		vm_get_page_mapping(vm_get_kernel_aspace(), (addr)new_map->arch_data->pgdir_virt, (addr *)&new_map->arch_data->pgdir_phys);
		new_map->arch_data->pgdir_phys = PHYS_TO_P1(new_map->arch_data->pgdir_phys);
		// zero out the new pgdir
		memset((void *)new_map->arch_data->pgdir_virt, 0, PAGE_SIZE);
		new_map->arch_data->is_user = true;
	} else {
		// kernel
		// we already know the kernel pgdir mapping
		(addr)new_map->arch_data->pgdir_virt = NULL;
		(addr)new_map->arch_data->pgdir_phys = vcpu->kernel_pgdir;
		new_map->arch_data->is_user = false;
	}
	
	return 0;
}

int vm_translation_map_module_init(kernel_args *ka)
{
	vcpu = ka->arch_args.vcpu;
	
	return 0;
}

int vm_translation_map_module_init2(kernel_args *ka)
{
	return 0;
}

// XXX horrible back door to map a page quickly regardless of translation map object, etc.
// used only during VM setup
int vm_translation_map_quick_map(kernel_args *ka, addr va, addr pa, unsigned int lock, addr (*get_free_page)(kernel_args *))
{
	struct pdent *pd = NULL;
	struct ptent *pt;
	unsigned int index;

#if CHATTY_TMAP
	dprintf("quick_tmap: va 0x%x pa 0x%x lock 0x%x\n", va, pa, lock);
#endif

	if(va < P3_AREA && va >= P4_AREA) {
		panic("quick_tmap: asked to map invalid va 0x%x\n", va);
	}
	pd = (struct pdent *)vcpu->kernel_pgdir;
	index = (va & 0x7fffffff) >> 22;

	if(pd[index].v == 0) {
		vm_page *page;
		unsigned int pgtable;

		pgtable = (*get_free_page)(ka) * PAGE_SIZE;
		
		memset((void *)PHYS_TO_P1(pgtable), 0, PAGE_SIZE);
		
		pd[index].ppn = pgtable >> 12;
		pd[index].v = 1;
	}

	// get the pagetable
	pt = (struct ptent *)PHYS_TO_P1(pd[index].ppn << 12);
	index = (va >> 12) & 0x000003ff;

	// insert the mapping
	pt[index].wt = 0;
	pt[index].pr = (lock & LOCK_KERNEL) ? (lock & 0x1) : (lock | 0x2);
	pt[index].ppn = pa >> 12;
	pt[index].tlb_ent = 0;
	pt[index].c = 1;
	pt[index].sz = 0x1; // 4k page
	pt[index].sh = 0;
	pt[index].d = 0;
	pt[index].v = 1;
		
	invl_page(va);

	return 0;
}

addr vm_translation_map_get_pgdir(vm_translation_map *map)
{
	return (addr)map->arch_data->pgdir_phys;
}
