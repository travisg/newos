#include <string.h>
#include <printf.h>
#include "vm.h"
#include "kernel.h"
#include "debug.h"
#include "stage2.h"

#include "arch_cpu.h"
#include "arch_pmap.h"
#include "arch_vm.h"

#define KERNEL_BASE 0x80000000
#define KERNEL_SIZE 0x80000000

static unsigned int first_free_page_index = 0;
static unsigned int *free_page_table = NULL;
static unsigned int free_page_table_size = 0;

#define END_OF_LIST 0xffffffff
#define PAGE_INUSE  0xfffffffe

#if 0
static void dump_free_page_table();
#endif

// heap stuff
// ripped mostly from nujeffos
#define HEAP_BASE	0x80400000
#define HEAP_SIZE	0x00400000

struct heap_page {
	unsigned short bin_index : 5;
	unsigned short free_count : 9;
	unsigned short cleaning : 1;
	unsigned short in_use : 1;
} PACKED;

static struct heap_page *heap_alloc_table = (struct heap_page *)HEAP_BASE;
static int heap_base = PAGE_ALIGN(HEAP_BASE + (HEAP_SIZE / PAGE_SIZE) * sizeof(struct heap_page));
struct heap_bin {
	int element_size;
	int grow_size;
	int alloc_count;
	void *free_list;
	int free_count;
	char *raw_list;
	int raw_count;
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

static struct aspace *aspace_list = NULL;
static struct aspace *kernel_aspace = NULL;
static int next_aspace_id = 0;

static int next_area_id = 0;

struct area *vm_find_area_by_name(struct aspace *aspace, const char *name)
{
	struct area *area;
	
	area = aspace->area_list;
	while(area != NULL) {
		if(strcmp(area->name, name) == 0)
			return area;
		area = area->next;
	}
	return NULL;
}

struct area *_vm_create_area_struct(struct aspace *aspace, char *name, unsigned int base, unsigned int size, unsigned int lock)
{
	struct area *area;
	struct area *t, *last = NULL;
	
	// allocate an area struct to represent this area
	area = (struct area *)kmalloc(sizeof(struct area));	
	area->base = base;
	area->size = size;
	area->id = next_area_id++;
	area->name = (char *)kmalloc(strlen(name) + 1);
	strcpy(area->name, name);
	
	// insert into the list
	// we'll need to search for the spot
	// check for address overlaps
	t = aspace->area_list;
	while(t != NULL) {
		if(t->base > base) {
			if(base + size > t->base) {
				// overlap
				kfree(area->name);
				kfree(area);
				return NULL;
			}
			area->next = t;
			if(last == NULL)
				aspace->area_list = area;
			else
				last->next = area;
			break;
		}
		last = t;
		t = t->next;	
	}
	if(t == NULL) {
		area->next = NULL;
		if(last == NULL)
			aspace->area_list = area;
		else
			last->next = area;
	}
	aspace->area_count++;

	return area;
}

enum {
	SRC_ADDR_ANY = 0,
	SRC_ADDR_PHYSICAL,
	SRC_ADDR_MAPPED_ALREADY
};

static int _vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock,
	unsigned int src_addr, unsigned int src_addr_type)
{
	struct area *area;
	unsigned int base;

	switch(addr_type) {
		case AREA_ANY_ADDRESS: {
			struct area *a;
			struct area *next_a = NULL;
			// find a hole big enough for a new area
	
			base = 0;
	
			a = aspace->area_list;
			if(a == NULL)
				base = aspace->base;
			else 
				next_a = a->next;
			while(a != NULL) {
				if(next_a != NULL) {
					if(next_a->base - (a->base + a->size) >= size) {
						// we have a spot
						base = a->base + a->size;
						break;
					}
				} else {
					if((aspace->base + aspace->size) - (a->base + a->size) + 1 >= size) {
						// we have a spot
						base = a->base + a->size;
						break;
					}
				}
				a = next_a;
				if(next_a != NULL)
					next_a = next_a->next;
			}

			if(base == 0)
				return -1;
			*addr = (void *)base;
			area = _vm_create_area_struct(aspace, name, base, size, lock);
			break;
		}
		case AREA_EXACT_ADDRESS:
			base = (unsigned int)*addr;
			area = _vm_create_area_struct(aspace, name, base, size, lock);
			break;
		default:
			// whut?
			area = NULL;
			return -1;
	}

	// ok, now we've allocated the area descriptor and put it in place,
	// lets find the pages
	switch(src_addr_type) {
		case SRC_ADDR_ANY: {
			int i;
			unsigned int page;

			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				vm_get_free_page(&page);
				vm_mark_page_inuse(page);
				pmap_map_page(page * PAGE_SIZE, base + i * PAGE_SIZE); 
			}
			break;
		}
		case SRC_ADDR_PHYSICAL: {
			int i;
			
			vm_mark_page_range_inuse(src_addr / PAGE_SIZE, PAGE_ALIGN(size) / PAGE_SIZE);
			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				pmap_map_page(src_addr + i * PAGE_SIZE, base + i * PAGE_SIZE); 
			}
			break;
		}
		case SRC_ADDR_MAPPED_ALREADY:
			break;
		default:
			// hmmm
			return -1;
	}
	
	return area->id;
}

int vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock)
{
	if(addr_type == AREA_ALREADY_MAPPED) 
		return _vm_create_area(aspace, name, addr, AREA_EXACT_ADDRESS, size, lock, 0, SRC_ADDR_MAPPED_ALREADY);
	else
		return _vm_create_area(aspace, name, addr, addr_type, size, lock, 0, 0);
}

int vm_map_physical_memory(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, unsigned int phys_addr)
{
	return _vm_create_area(aspace, name, addr, addr_type, size, lock, phys_addr, SRC_ADDR_PHYSICAL);
}

static void vm_dump_areas(struct aspace *aspace)
{
	struct area *area;

	dprintf("area dump of address space '%s':\n", aspace->name);
	
	area = aspace->area_list;
	while(area != NULL) {
		dprintf("area 0x%x: ", area->id);
		dprintf("base_addr = 0x%x ", area->base);
		dprintf("size = 0x%x ", area->size);
		dprintf("name = '%s'\n", area->name);
		area = area->next;
	}
}

struct aspace *vm_get_kernel_aspace()
{
	return kernel_aspace;
}

struct aspace *vm_create_aspace(const char *name, unsigned int base, unsigned int size)
{
	struct aspace *aspace;
	
	aspace = (struct aspace *)kmalloc(sizeof(struct aspace));
	if(aspace == NULL)
		return NULL;
	aspace->id = next_aspace_id++;
	aspace->name = (char *)kmalloc(strlen(name) + 1);
	if(aspace->name == NULL ) {
		kfree(aspace);
		return NULL;
	}
	strcpy(aspace->name, name);
	aspace->base = base;
	aspace->size = size;
	aspace->area_list = NULL;
	aspace->area_count = 0;

	// insert it into the aspace list
	aspace->next = aspace_list;
	aspace_list = aspace;

	return aspace;	
}

int vm_init(struct kernel_args *ka)
{
	int err;
	unsigned int i;

	dprintf("vm_init: entry\n");

	err = arch_pmap_init(ka);
	err = arch_vm_init(ka);
	
	// set up the free page table immediately past what has been allocated
	free_page_table = (unsigned int *)ka->virt_alloc_range_high;
	free_page_table_size = ka->mem_size/PAGE_SIZE;
	// map in this new table
	ka->phys_alloc_range_high = PAGE_ALIGN(ka->phys_alloc_range_high);
	for(i = 0; i < free_page_table_size/(PAGE_SIZE/sizeof(unsigned int)); i++) {
		map_page_into_kspace(ka->phys_alloc_range_high, ka->virt_alloc_range_high);
		ka->phys_alloc_range_high += PAGE_SIZE;
		ka->virt_alloc_range_high += PAGE_SIZE;
	}
	dprintf("vm_init: putting free_page_table @ %p, # ents %d\n",
		free_page_table, free_page_table_size);
	
	// initialize the free page table
	for(i=0; i<free_page_table_size-1; i++) {
		free_page_table[i] = i+1;
	}
	free_page_table[i] = END_OF_LIST;
	first_free_page_index = 0;

	// mark some of the page ranges inuse
	vm_mark_page_range_inuse(ka->phys_alloc_range_low/PAGE_SIZE,
		ka->phys_alloc_range_high/PAGE_SIZE - ka->phys_alloc_range_low/PAGE_SIZE);
	
	// map in the new heap
	for(i = HEAP_BASE; i < HEAP_BASE + HEAP_SIZE; i += PAGE_SIZE) {
		map_page_into_kspace(ka->phys_alloc_range_high, i);
		vm_mark_page_inuse(ka->phys_alloc_range_high / PAGE_SIZE);
		ka->phys_alloc_range_high += PAGE_SIZE;
	}

	// zero out the heap alloc table at the base of the heap
	memset((void *)heap_alloc_table, 0, (HEAP_SIZE / PAGE_SIZE) * sizeof(struct heap_page));

	// create the initial kernel address space
	kernel_aspace = vm_create_aspace("kernel_land", KERNEL_BASE, KERNEL_SIZE);

	// do any further initialization that the architecture dependant layers may need now
	arch_pmap_init2(ka);
	arch_vm_init2(ka);

	// allocate areas to represent stuff that already exists
	_vm_create_area_struct(kernel_aspace, "kernel_heap", (unsigned int)HEAP_BASE, HEAP_SIZE, 0);
	_vm_create_area_struct(kernel_aspace, "free_page_table", (unsigned int)free_page_table, free_page_table_size * sizeof(unsigned int), 0);
	_vm_create_area_struct(kernel_aspace, "kernel_seg0", (unsigned int)ka->kernel_seg0_base, ka->kernel_seg0_size, 0);
	_vm_create_area_struct(kernel_aspace, "kernel_seg1", (unsigned int)ka->kernel_seg1_base, ka->kernel_seg1_size, 0);
	_vm_create_area_struct(kernel_aspace, "idle_thread_kstack", (unsigned int)ka->stack_start, ka->stack_end - ka->stack_start, 0);

	vm_dump_areas(kernel_aspace);

	dprintf("vm_init: exit\n");

	return err;
}

static char *raw_alloc(int size, int bin_index)
{
	unsigned int new_heap_ptr;
	char *retval;
	struct heap_page *page;
	unsigned int addr;
	
	new_heap_ptr = heap_base + PAGE_ALIGN(size);
	if(new_heap_ptr > HEAP_BASE + HEAP_SIZE) {
		dprintf("heap overgrew itself! spinning forever...\n");
		for(;;);
	}

	for(addr = heap_base; addr < heap_base + size; addr += PAGE_SIZE) {
		page = &heap_alloc_table[(addr - HEAP_BASE) / PAGE_SIZE];
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

void *kmalloc(int size)
{
	void *address = 0;
	int bin_index;
	unsigned int i;
	struct heap_page *page;
	
	for (bin_index = 0; bin_index < bin_count; bin_index++)
		if (size <= bins[bin_index].element_size)
			break;

	if (bin_index == bin_count) {
		// XXX fix the raw alloc later.
		//address = raw_alloc(size, bin_index);
		return NULL;
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
		page = &heap_alloc_table[((unsigned int)address - HEAP_BASE) / PAGE_SIZE];
		for(i = 0; i < bins[bin_index].element_size / PAGE_SIZE; i++)
			page[i].free_count--;
	}

//	dprintf("kmalloc: asked to allocate size %d, returning ptr = %p\n", size, address);

	return address;
}

void kfree(void *address)
{
	struct heap_page *page;
	struct heap_bin *bin;
	unsigned int i;

	if (address == 0)
		return;

//	dprintf("kfree: asked to free at ptr = %p\n", address);

	page = &heap_alloc_table[((unsigned)address - HEAP_BASE) / PAGE_SIZE];
	
	// XXX fix later
	if(page[0].bin_index == bin_count)
		return;

	bin = &bins[page[0].bin_index];

	for(i = 0; i < bin->element_size / PAGE_SIZE; i++)
		page[i].free_count++;

	*(unsigned int *)address = (unsigned int)bin->free_list;
	bin->free_list = address;
	bin->alloc_count--;
	bin->free_count++;
}

int vm_mark_page_range_inuse(unsigned int start_page, unsigned int len)
{
	unsigned int i;
	unsigned int last_i;
	unsigned int j;
	
#if 0
	dprintf("vm_mark_page_range_inuse: entry. start_page %d len %d\n", start_page, len);
#endif
	if(start_page + len >= free_page_table_size) {
		dprintf("vm_mark_page_range_inuse: range would extend past free list\n");
		return -1;
	}

	// walk thru the free page list to find the spot
	last_i = END_OF_LIST;
	i = first_free_page_index;
	while(i != END_OF_LIST && i < start_page) {
		last_i = i;
		i = free_page_table[i];
	}
	
	if(i == END_OF_LIST || i > start_page) {
		dprintf("vm_mark_page_range_inuse: could not find start_page in free_page_list\n");
		return -1;
	}
				
	for(j=i; j<(len + i); j++) {
		if(free_page_table[j] == PAGE_INUSE) {
			dprintf("vm_mark_page_range_inuse: found page inuse already\n");
		}
		free_page_table[j] = PAGE_INUSE;
	}

	if(first_free_page_index == i) {
		first_free_page_index = j;
	} else {
		free_page_table[last_i] = j;
	}

	return 0;
}

int vm_mark_page_inuse(unsigned int page)
{
	return vm_mark_page_range_inuse(page, 1);
}

int vm_get_free_page(unsigned int *page)
{
	int index = first_free_page_index;
	
	dprintf("vm_get_free_page entry\n");
		
	if(index == END_OF_LIST)
		return -1;

	*page = index;
	first_free_page_index = free_page_table[index];
	free_page_table[index] = PAGE_INUSE;
	dprintf("vm_get_free_page exit, returning page 0x%x\n", *page);
	return 0;
}

#if 0
static void dump_free_page_table()
{
	unsigned int i = 0;
	unsigned int free_start = END_OF_LIST;
	unsigned int inuse_start = PAGE_INUSE;
	
	dprintf("first_free_page_index = %d\n", first_free_page_index);

	while(i < free_page_table_size) {
		if(free_page_table[i] == PAGE_INUSE) {
			if(inuse_start != PAGE_INUSE) {
				i++;
				continue;
			}
			if(free_start != END_OF_LIST) {
				dprintf("free from %d -> %d\n", free_start, i-1);
				free_start = END_OF_LIST;
			}
			inuse_start = i;
		} else {
			if(free_start != END_OF_LIST) {
				i++;
				continue;
			}
			if(inuse_start != PAGE_INUSE) {
				dprintf("inuse from %d -> %d\n", inuse_start, i-1);
				inuse_start = PAGE_INUSE;
			}
			free_start = i;
		}
		i++;
	}
	if(inuse_start != PAGE_INUSE) {
		dprintf("inuse from %d -> %d\n", inuse_start, i-1);
	}
	if(free_start != END_OF_LIST) {
		dprintf("free from %d -> %d\n", free_start, i-1);
	}
/*			
	for(i=0; i<free_page_table_size; i++) {
		dprintf("%d->%d ", i, free_page_table[i]);
	}
*/
}
#endif

void vm_page_fault(int address, int errorcode)
{
	dprintf("PAGE FAULT: address 0x%x. Killing system.\n", address);

//	cli();
	for(;;);
}

