#include <string.h>
#include <ctype.h>
#include <printf.h>
#include <kernel.h>
#include <vm.h>
#include <debug.h>
#include <console.h>
#include <stage2.h>
#include <int.h>
#include <smp.h>
#include <sem.h>

#include <arch_cpu.h>
#include <arch_pmap.h>
#include <arch_vm.h>

static unsigned int first_free_page_index = 0;
static unsigned int *free_page_table = NULL;
static unsigned int free_page_table_size = 0;

#define END_OF_LIST 0xffffffff
#define PAGE_INUSE  0xfffffffe

static void dump_free_page_table(int argc, char **argv);

// heap stuff
// ripped mostly from nujeffos
#define HEAP_BASE	(KERNEL_BASE + 0x400000)
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

static struct aspace *aspace_list = NULL;
static struct aspace *kernel_aspace = NULL;
static aspace_id next_aspace_id = 0;

static area_id next_area_id = 0;

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
	struct area *a, *last = NULL;
	
	// allocate an area struct to represent this area
	area = (struct area *)kmalloc(sizeof(struct area));	
	area->base = base;
	area->size = size;
	area->id = next_area_id++;
	area->lock = lock;
	area->name = (char *)kmalloc(strlen(name) + 1);
	strcpy(area->name, name);
	
	// insert into the list
	// we'll need to search for the spot
	// check for address overlaps
	a = aspace->area_list;
	while(a != NULL) {
//		dprintf("create_area_struct: a = 0x%x. base = 0x%x, size = 0x%x\n", a, a->base, a->size);
		if(a->base > base) {
			if(base + size > a->base) {
				// overlap
				kfree(area->name);
				kfree(area);
				return NULL;
			}
			area->next = a;
			if(last == NULL)
				aspace->area_list = area;
			else
				last->next = area;
			break;
		}
		last = a;
		a = a->next;	
	}
	if(a == NULL) {
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

static area_id _vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock,
	unsigned int src_addr, unsigned int src_addr_type)
{
	struct area *area;
	unsigned int base;

	dprintf("_vm_create_area: '%s', *addr = 0x%p, addr_type = %d, size = %d, src_addr = 0x%x\n",
		name, *addr, addr_type, size, src_addr);

	// check validity of lock
	if((lock & ~LOCK_MASK) != 0) {
		// invalid lock
		panic("_vm_create_area called with invalid lock %d\n", lock);
	}
	
	switch(addr_type) {
		case AREA_ANY_ADDRESS: {
			struct area *a;
			struct area *next_a = NULL;
			// find a hole big enough for a new area
	
			base = 0;
	
			a = aspace->area_list;
			if(a == NULL) {
				base = aspace->base;
			} else if(a != NULL && a->base > aspace->base) {
				// lets try to build the area at the beginning of the aspace
				if(aspace->base + size > a->base) {
					// if we built it here, it would overlap, so let the loop below
					// find the right spot
					next_a = a->next;
				} else {
					// otherwise, we're done.
					base = aspace->base;
					a = NULL;
				}
			} else {
				next_a = a->next;
			}
			while(a != NULL) {
//				dprintf("a = 0x%x. base = 0x%x, size = 0x%x\n", a, a->base, a->size);
				if(next_a != NULL) {
//					dprintf("next_a = 0x%x. base = 0x%x, size = 0x%x\n", next_a, next_a->base, next_a->size);
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
			
			if(base < aspace->base || (base - aspace->base) + size > (base - aspace->base) + aspace->size) {
				dprintf("_vm_create_area: area asked to be created outside of aspace\n");
				return -1;
			}
			
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
			unsigned int i;
			unsigned int page;

			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				vm_get_free_page(&page);
				pmap_map_page(page * PAGE_SIZE, base + i * PAGE_SIZE, lock); 
			}
			break;
		}
		case SRC_ADDR_PHYSICAL: {
			unsigned int i;
			
			vm_mark_page_range_inuse(src_addr / PAGE_SIZE, PAGE_ALIGN(size) / PAGE_SIZE);
			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				pmap_map_page(src_addr + i * PAGE_SIZE, base + i * PAGE_SIZE, lock); 
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

area_id vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
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

static void display_mem(int argc, char **argv)
{
	int item_size;
	int display_width;
	int num = 1;
	addr address;
	int i;
	int j;
	
	if(argc < 2) {
		dprintf("not enough arguments\n");
		return;
	}
	
	address = atoul(argv[1]);
	
	if(argc >= 3) {
		num = -1;
		num = atoi(argv[2]);
	}

	// build the format string
	if(strcmp(argv[0], "db") == 0) {
		item_size = 1;
		display_width = 16;
	} else if(strcmp(argv[0], "ds") == 0) {
		item_size = 2;
		display_width = 8;
	} else if(strcmp(argv[0], "dw") == 0) {
		item_size = 4;
		display_width = 4;
	} else {
		dprintf("display_mem called in an invalid way!\n");
		return;
	}

	dprintf("[0x%x] '", address);
	for(j=0; j<min(display_width, num) * item_size; j++) {
		char c = *((char *)address + j);
		if(!isalnum(c)) {
			c = '.';
		}
		dprintf("%c", c);
	}
	dprintf("'");
	for(i=0; i<num; i++) {	
		if((i % display_width) == 0 && i != 0) {
			dprintf("\n[0x%x] '", address + i * item_size);
			for(j=0; j<min(display_width, (num-i)) * item_size; j++) {
				char c = *((char *)address + i * item_size + j);
				if(!isalnum(c)) {
					c = '.';
				}
				dprintf("%c", c);
			}
			dprintf("'");
		}
		
		switch(item_size) {
			case 1:
				dprintf(" 0x%02x", *((uint8 *)address + i));
				break;
			case 2:
				dprintf(" 0x%04x", *((uint16 *)address + i));
				break;
			case 4:
				dprintf(" 0x%08x", *((uint32 *)address + i));
				break;
			default:
				dprintf("huh?\n");
		}
	}
	dprintf("\n");
}

static void vm_dump_kspace_areas(int argc, char **argv)
{
	vm_dump_areas(vm_get_kernel_aspace());
}

void vm_dump_areas(struct aspace *aspace)
{
	struct area *area;

	dprintf("area dump of address space '%s', base 0x%x, size 0x%x:\n", aspace->name, aspace->base, aspace->size);
	
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

int vm_init(kernel_args *ka)
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
		map_page_into_kspace(ka->phys_alloc_range_high, ka->virt_alloc_range_high, LOCK_RW|LOCK_KERNEL);
		ka->phys_alloc_range_high += PAGE_SIZE;
		ka->virt_alloc_range_high += PAGE_SIZE;
	}
//	dprintf("vm_init: putting free_page_table @ %p, # ents %d\n",
//		free_page_table, free_page_table_size);
	
	// initialize the free page table
	for(i=0; i<free_page_table_size-1; i++) {
		free_page_table[i] = i+1;
	}
	free_page_table[i] = END_OF_LIST;
	first_free_page_index = 0;

	// map in the new heap
	for(i = HEAP_BASE; i < HEAP_BASE + HEAP_SIZE; i += PAGE_SIZE) {
		map_page_into_kspace(ka->phys_alloc_range_high, i, LOCK_RW | LOCK_KERNEL);
		ka->phys_alloc_range_high += PAGE_SIZE;
	}

	// mark some of the page ranges inuse
	vm_mark_page_range_inuse(ka->phys_alloc_range_low/PAGE_SIZE,
		ka->phys_alloc_range_high/PAGE_SIZE - ka->phys_alloc_range_low/PAGE_SIZE);
	
	// zero out the heap alloc table at the base of the heap
	memset((void *)heap_alloc_table, 0, (HEAP_SIZE / PAGE_SIZE) * sizeof(struct heap_page));

	// create the initial kernel address space
	kernel_aspace = vm_create_aspace("kernel_land", KERNEL_BASE, KERNEL_SIZE);

	// do any further initialization that the architecture dependant layers may need now
	arch_pmap_init2(ka);
	arch_vm_init2(ka);

	// allocate areas to represent stuff that already exists
	_vm_create_area_struct(kernel_aspace, "kernel_heap", (unsigned int)HEAP_BASE, HEAP_SIZE, LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "free_page_table", (unsigned int)free_page_table, free_page_table_size * sizeof(unsigned int), LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "kernel_seg0", (unsigned int)ka->kernel_seg0_base, ka->kernel_seg0_size, LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "kernel_seg1", (unsigned int)ka->kernel_seg1_base, ka->kernel_seg1_size, LOCK_RW|LOCK_KERNEL);
	for(i=0; i < ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d_kstack", i);
		_vm_create_area_struct(kernel_aspace, temp, (unsigned int)ka->cpu_kstack[i], ka->cpu_kstack_len[i], LOCK_RW|LOCK_KERNEL);
	}
	{
		void *null;
		vm_map_physical_memory(kernel_aspace, "bootdir", &null, AREA_ANY_ADDRESS,
			ka->bootdir_size, LOCK_RO|LOCK_KERNEL, (unsigned int)ka->bootdir);
	}
//	vm_dump_areas(kernel_aspace);

	// add some debugger commands
	dbg_add_command(&vm_dump_kspace_areas, "area_dump_kspace", "Dump kernel space areas");
	dbg_add_command(&dump_free_page_table, "free_pages", "Dump free page table list");	
//	dbg_add_command(&display_mem, "dl", "dump memory long words (64-bit)");
	dbg_add_command(&display_mem, "dw", "dump memory words (32-bit)");
	dbg_add_command(&display_mem, "ds", "dump memory shorts (16-bit)");
	dbg_add_command(&display_mem, "db", "dump memory bytes (8-bit)");

	dprintf("vm_init: exit\n");

	return err;
}

int vm_init_postsem(kernel_args *ka)
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
		page = &heap_alloc_table[((unsigned int)address - HEAP_BASE) / PAGE_SIZE];
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

	page = &heap_alloc_table[((unsigned)address - HEAP_BASE) / PAGE_SIZE];
	
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

int vm_mark_page_range_inuse(unsigned int start_page, unsigned int len)
{
	unsigned int i;
	unsigned int last_i;
	unsigned int j;
	
#if 0
	dprintf("vm_mark_page_range_inuse: entry. start_page %d len %d, first_free %d\n",
		start_page, len, first_free_page_index);
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
		dprintf("vm_mark_page_range_inuse: could not find start_page (%d) in free_page_list\n", start_page);
		dump_free_page_table(0, NULL);
		return -1;
	}
				
	for(j=i; j<(len + i); j++) {
		if(free_page_table[j] == PAGE_INUSE) {
			dprintf("vm_mark_page_range_inuse: found page inuse already\n");	
			dump_free_page_table(0, NULL);
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
	unsigned int index = first_free_page_index;
	
//	dprintf("vm_get_free_page entry\n");
		
	if(index == END_OF_LIST)
		return -1;

	*page = index;
	first_free_page_index = free_page_table[index];
	free_page_table[index] = PAGE_INUSE;
//	dprintf("vm_get_free_page exit, returning page 0x%x\n", *page);
	return 0;
}

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

int vm_page_fault(int address, unsigned int fault_address)
{
	dprintf("PAGE FAULT: faulted on address 0x%x. ip = 0x%x. Killing system.\n", address, fault_address);
	
	panic("page fault\n");
//	cli();
	for(;;);
	return INT_NO_RESCHEDULE;
}

