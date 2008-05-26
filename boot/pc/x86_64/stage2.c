/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/bootdir.h>
#include <boot/stage2.h>
#include "stage2_priv.h"
#include "vesa.h"
#include "int86.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <newos/elf32.h>

// we're running out of the first 'file' contained in the bootdir, which is
// a set of binaries and data packed back to back, described by an array
// of boot_entry structures at the beginning. The load address is fixed.
#define BOOTDIR_ADDR 0x400000
static const boot_entry *bootdir = (boot_entry*)BOOTDIR_ADDR;

// stick the kernel arguments in a pseudo-random page that will be mapped
// at least during the call into the kernel. The kernel should copy the
// data out and unmap the page.
static kernel_args *ka = (kernel_args *)0x20000;

// needed for message
static unsigned short *kScreenBase = (unsigned short*) 0xb8000;
static unsigned screenOffset = 0;

unsigned int cv_factor = 0;

// size of bootdir in pages
static unsigned int bootdir_pages = 0;

// working pagedir and pagetable
static unsigned int *pgdir = 0;
static unsigned int *pgtable = 0;

// function decls for this module
static void calculate_cpu_conversion_factor(void);
static void load_elf_image(void *data, unsigned int *next_paddr,
	addr_range *ar0, addr_range *ar1, unsigned int *start_addr, addr_range *dynamic_section);
static int mmu_init(kernel_args *ka, unsigned int *next_paddr);
static void mmu_map_page(unsigned int vaddr, unsigned int paddr);
static int check_cpu(kernel_args *ka);
static void sort_addr_range(addr_range *range, int count);

// memory structure returned by int 0x15, ax 0xe820
struct emem_struct {
	uint64 base_addr;
	uint64 length;
	uint64 type;
	uint64 filler;
};

// called by the stage1 bootloader.
// State:
//   32-bit
//   mmu disabled
//   stack somewhere below 1 MB
//   supervisor mode
void _start(unsigned int memsize, void *extended_mem_block, unsigned int extended_mem_count, int in_vesa, unsigned int vesa_ptr, unsigned int console_ptr)
{
	unsigned int *idt;
	unsigned int *gdt;
	unsigned int next_vaddr;
	unsigned int next_paddr;
	unsigned int i;
	unsigned int kernel_entry;

	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	screenOffset = console_ptr;
	dprintf("stage2 bootloader entry.\n");
	dprintf("args: memsize 0x%x, emem_block %p, emem_count %d, in_vesa %d\n", 
		memsize, extended_mem_block, extended_mem_count, in_vesa);
	
	for (;;);

	// verify we can run on this cpu
	if(check_cpu(ka) < 0) {
		dprintf("\nSorry, this computer appears to be lacking some of the features\n");
		dprintf("needed by NewOS.\n");
		dprintf("\nPlease reset your computer to continue.");

		for(;;);
	}

	if(extended_mem_count > 0) {
 		struct emem_struct *buf = (struct emem_struct *)extended_mem_block;
		unsigned int i;

		dprintf("extended memory info (from 0xe820):\n");
		for(i=0; i<extended_mem_count; i++) {
			dprintf("    base 0x%Lx, len 0x%Lx, type %Ld\n", 
				buf[i].base_addr, buf[i].length, buf[i].type);
		}
	}

	// calculate the conversion factor that translates rdtsc time to real microseconds
	if(ka->arch_args.supports_rdtsc)
		calculate_cpu_conversion_factor();

	// calculate how big the bootdir is so we know where we can start grabbing pages
	{
		int entry;
		for (entry = 0; entry < BOOTDIR_MAX_ENTRIES; entry++) {
			if (bootdir[entry].be_type == BE_TYPE_NONE)
				break;

			bootdir_pages += bootdir[entry].be_size;
		}

//		nmessage("bootdir is ", bootdir_pages, " pages long\n");
	}

	ka->bootdir_addr.start = (unsigned long)bootdir;
	ka->bootdir_addr.size = bootdir_pages * PAGE_SIZE;

	next_paddr = BOOTDIR_ADDR + bootdir_pages * PAGE_SIZE;

	if(in_vesa) {
		//struct VBEInfoBlock *info = (struct VBEInfoBlock *)vesa_ptr;
		struct VBEModeInfoBlock *mode_info = (struct VBEModeInfoBlock *)(vesa_ptr + 0x200);

		ka->fb.enabled = 1;
		ka->fb.x_size = mode_info->x_resolution;
		ka->fb.y_size = mode_info->y_resolution;
		ka->fb.bit_depth = mode_info->bits_per_pixel;
		ka->fb.red_mask_size = mode_info->red_mask_size;
		ka->fb.red_field_position = mode_info->red_field_position;
		ka->fb.green_mask_size = mode_info->green_mask_size;
		ka->fb.green_field_position = mode_info->green_field_position;
		ka->fb.blue_mask_size = mode_info->blue_mask_size;
		ka->fb.blue_field_position = mode_info->blue_field_position;
		ka->fb.reserved_mask_size = mode_info->reserved_mask_size;
		ka->fb.reserved_field_position = mode_info->reserved_field_position;
		ka->fb.mapping.start = mode_info->phys_base_ptr;
		ka->fb.mapping.size = ka->fb.x_size * ka->fb.y_size * ((ka->fb.bit_depth+7)/8);
		ka->fb.already_mapped = 0;
	} else {
		ka->fb.enabled = 0;
	}

	mmu_init(ka, &next_paddr);

	// load the kernel (3rd entry in the bootdir)
	load_elf_image((void *)(bootdir[2].be_offset * PAGE_SIZE + BOOTDIR_ADDR), &next_paddr,
			&ka->kernel_seg0_addr, &ka->kernel_seg1_addr, &kernel_entry, &ka->kernel_dynamic_section_addr);

	if(ka->kernel_seg1_addr.size > 0)
		next_vaddr = ROUNDUP(ka->kernel_seg1_addr.start + ka->kernel_seg1_addr.size, PAGE_SIZE);
	else
		next_vaddr = ROUNDUP(ka->kernel_seg0_addr.start + ka->kernel_seg0_addr.size, PAGE_SIZE);

	// map in a kernel stack
	ka->cpu_kstack[0].start = next_vaddr;
	for(i=0; i<STACK_SIZE; i++) {
		mmu_map_page(next_vaddr, next_paddr);
		next_vaddr += PAGE_SIZE;
		next_paddr += PAGE_SIZE;
	}
	ka->cpu_kstack[0].size = next_vaddr - ka->cpu_kstack[0].start;

//	dprintf("new stack at 0x%x to 0x%x\n", ka->cpu_kstack[0].start, ka->cpu_kstack[0].start + ka->cpu_kstack[0].size);

	// set up a new gdt
	{
		struct gdt_idt_descr gdt_descr;

		// find a new gdt
		gdt = (unsigned int *)next_paddr;
		ka->arch_args.phys_gdt = (unsigned int)gdt;
		next_paddr += PAGE_SIZE;

//		nmessage("gdt at ", (unsigned int)gdt, "\n");

		// put segment descriptors in it
		gdt[0] = 0;
		gdt[1] = 0;
		gdt[2] = 0x0000ffff; // seg 0x8  -- kernel 4GB code
		gdt[3] = 0x00cf9a00;
		gdt[4] = 0x0000ffff; // seg 0x10 -- kernel 4GB data
		gdt[5] = 0x00cf9200;
		gdt[6] = 0x0000ffff; // seg 0x1b -- ring 3 4GB code
		gdt[7] = 0x00cffa00;
		gdt[8] = 0x0000ffff; // seg 0x23 -- ring 3 4GB data
		gdt[9] = 0x00cff200;
		// gdt[10] & gdt[11] will be filled later by the kernel

		// map the gdt into virtual space
		mmu_map_page(next_vaddr, (unsigned int)gdt);
		ka->arch_args.vir_gdt = (unsigned int)next_vaddr;
		next_vaddr += PAGE_SIZE;

		// load the GDT
		gdt_descr.a = GDT_LIMIT - 1;
		gdt_descr.b = (unsigned int *)ka->arch_args.vir_gdt;

		asm("lgdt	%0;"
			: : "m" (gdt_descr));

//		nmessage("gdt at virtual address ", next_vpage, "\n");
	}

#if 0
	{
		struct regs r;
		struct ebios_struct {
			uint32 base_addr_low;
			uint32 base_addr_high;
			uint32 length_low;
			uint32 length_high;
			uint8 type;
		} *buf = (struct ebios_struct *)0xf000;

		memset(buf, 0, sizeof(struct ebios_struct));

		r.ebx = 0;
		do {
			r.eax = 0xe820;
			r.ecx = 0x20;
			r.edx = 'SMAP';
			r.esi = 0;
			r.edi = 0x1000;
			r.es = 0;
			r.flags = 0;

			int86(0x15, &r);

			if(r.flags & 0x1) {
				dprintf("error calling int 0x15 e820\n");
				break;
			}

			dprintf("flags 0x%x base 0x%x 0x%x length 0x%x 0x%x type %d\n",
				r.flags, buf->base_addr_low, buf->base_addr_high, buf->length_low, buf->length_high, buf->type);
			for(;;);
		} while(r.ebx != 0);
	}
#endif

	// set up a new idt
	{
		struct gdt_idt_descr idt_descr;

		// find a new idt
		idt = (unsigned int *)next_paddr;
		ka->arch_args.phys_idt = (unsigned int)idt;
		next_paddr += PAGE_SIZE;

//		nmessage("idt at ", (unsigned int)idt, "\n");

		// clear it out
		for(i=0; i<IDT_LIMIT/4; i++) {
			idt[i] = 0;
		}

		// map the idt into virtual space
		mmu_map_page(next_vaddr, (unsigned int)idt);
		ka->arch_args.vir_idt = (unsigned int)next_vaddr;
		next_vaddr += PAGE_SIZE;

		// load the idt
		idt_descr.a = IDT_LIMIT - 1;
		idt_descr.b = (unsigned int *)ka->arch_args.vir_idt;

		asm("lidt	%0;"
			: : "m" (idt_descr));

//		nmessage("idt at virtual address ", next_vpage, "\n");
	}

	// Map the pg_dir into kernel space at 0xffc00000-0xffffffff
	// this enables a mmu trick where the 4 MB region that this pgdir entry
	// represents now maps the 4MB of potential pagetables that the pgdir
	// points to. Thrown away later in VM bringup, but useful for now.
	pgdir[1023] = (unsigned int)pgdir | DEFAULT_PAGE_FLAGS;

	// also map it on the next vpage
	mmu_map_page(next_vaddr, (unsigned int)pgdir);
	ka->arch_args.vir_pgdir = next_vaddr;
	next_vaddr += PAGE_SIZE;

	// mark memory that we know is used
	ka->phys_alloc_range[0].start = BOOTDIR_ADDR;
	ka->phys_alloc_range[0].size = next_paddr - BOOTDIR_ADDR;
	ka->num_phys_alloc_ranges = 1;

	// figure out the memory map
	if(extended_mem_count > 0) {
		struct emem_struct *buf = (struct emem_struct *)extended_mem_block;
		unsigned int i;

		ka->num_phys_mem_ranges = 0;

		for(i = 0; i < extended_mem_count; i++) {
			if(buf[i].type == 1) {
				// round everything up to page boundaries, exclusive of pages it partially occupies
				buf[i].length -= (buf[i].base_addr % PAGE_SIZE) ? (PAGE_SIZE - (buf[i].base_addr % PAGE_SIZE)) : 0;
				buf[i].base_addr = ROUNDUP(buf[i].base_addr, PAGE_SIZE);
				buf[i].length = ROUNDOWN(buf[i].length, PAGE_SIZE);

				// this is mem we can use
				if(ka->num_phys_mem_ranges == 0) {
					ka->phys_mem_range[0].start = (addr_t)buf[i].base_addr;
					ka->phys_mem_range[0].size = (addr_t)buf[i].length;
					ka->num_phys_mem_ranges++;
				} else {
					// we might have to extend the previous hole
					addr_t previous_end = ka->phys_mem_range[ka->num_phys_mem_ranges-1].start + ka->phys_mem_range[ka->num_phys_mem_ranges-1].size;
					if(previous_end <= buf[i].base_addr && 
					  ((buf[i].base_addr - previous_end) < 0x100000)) {
						// extend the previous buffer
						ka->phys_mem_range[ka->num_phys_mem_ranges-1].size +=
							(buf[i].base_addr - previous_end) +
							buf[i].length;

						// mark the gap between the two allocated ranges in use
						ka->phys_alloc_range[ka->num_phys_alloc_ranges].start = previous_end;
						ka->phys_alloc_range[ka->num_phys_alloc_ranges].size = buf[i].base_addr - previous_end;
						ka->num_phys_alloc_ranges++;
					}
				}
			}
		}
	} else {
		// we dont have an extended map, assume memory is contiguously mapped at 0x0
		ka->phys_mem_range[0].start = 0;
		ka->phys_mem_range[0].size = memsize;
		ka->num_phys_mem_ranges = 1;

		// mark the bios area allocated
		ka->phys_alloc_range[ka->num_phys_alloc_ranges].start = 0x9f000; // 640k - 1 page
		ka->phys_alloc_range[ka->num_phys_alloc_ranges].size = 0x61000;
		ka->num_phys_alloc_ranges++;
	}

	// save the memory we've virtually allocated (for the kernel and other stuff)
	ka->virt_alloc_range[0].start = KERNEL_BASE;
	ka->virt_alloc_range[0].size = next_vaddr - KERNEL_BASE;
	ka->num_virt_alloc_ranges = 1;

	// sort the address ranges
	sort_addr_range(ka->phys_mem_range, ka->num_phys_mem_ranges);
	sort_addr_range(ka->phys_alloc_range, ka->num_phys_alloc_ranges);
	sort_addr_range(ka->virt_alloc_range, ka->num_virt_alloc_ranges);

#if 1
	{
		unsigned int i;

		dprintf("phys memory ranges:\n");
		for(i=0; i < ka->num_phys_mem_ranges; i++) {
			dprintf("    base 0x%08lx, length 0x%08lx\n", ka->phys_mem_range[i].start, ka->phys_mem_range[i].size);
		}

		dprintf("allocated phys memory ranges:\n");
		for(i=0; i < ka->num_phys_alloc_ranges; i++) {
			dprintf("    base 0x%08lx, length 0x%08lx\n", ka->phys_alloc_range[i].start, ka->phys_alloc_range[i].size);
		}

		dprintf("allocated virt memory ranges:\n");
		for(i=0; i < ka->num_virt_alloc_ranges; i++) {
			dprintf("    base 0x%08lx, length 0x%08lx\n", ka->virt_alloc_range[i].start, ka->virt_alloc_range[i].size);
		}
	}
#endif

	// save the kernel args
	ka->arch_args.system_time_cv_factor = cv_factor;
	ka->str = NULL;
	ka->arch_args.page_hole = 0xffc00000;
	ka->num_cpus = 1;
#if 0
	dprintf("kernel args at 0x%x\n", ka);
	dprintf("pgdir = 0x%x\n", ka->pgdir);
	dprintf("pgtables[0] = 0x%x\n", ka->pgtables[0]);
	dprintf("phys_idt = 0x%x\n", ka->phys_idt);
	dprintf("vir_idt = 0x%x\n", ka->vir_idt);
	dprintf("phys_gdt = 0x%x\n", ka->phys_gdt);
	dprintf("vir_gdt = 0x%x\n", ka->vir_gdt);
	dprintf("mem_size = 0x%x\n", ka->mem_size);
	dprintf("str = 0x%x\n", ka->str);
	dprintf("bootdir = 0x%x\n", ka->bootdir);
	dprintf("bootdir_size = 0x%x\n", ka->bootdir_size);
	dprintf("phys_alloc_range_low = 0x%x\n", ka->phys_alloc_range_low);
	dprintf("phys_alloc_range_high = 0x%x\n", ka->phys_alloc_range_high);
	dprintf("virt_alloc_range_low = 0x%x\n", ka->virt_alloc_range_low);
	dprintf("virt_alloc_range_high = 0x%x\n", ka->virt_alloc_range_high);
	dprintf("page_hole = 0x%x\n", ka->page_hole);
#endif
//	dprintf("finding and booting other cpus...\n");
	smp_boot(ka, kernel_entry);

	dprintf("jumping into kernel at 0x%x\n", kernel_entry);

	ka->cons_line = screenOffset / SCREEN_WIDTH;

	asm("movl	%0, %%eax;	"			// move stack out of way
		"movl	%%eax, %%esp; "
		: : "m" (ka->cpu_kstack[0].start + ka->cpu_kstack[0].size));
	asm("pushl  $0x0; "					// we're the BSP cpu (0)
		"pushl 	%0;	"					// kernel args
		"pushl 	$0x0;"					// dummy retval for call to main
		"pushl 	%1;	"					// this is the start address
		"ret;		"					// jump.
		: : "g" (ka), "g" (kernel_entry));
}

static void load_elf_image(void *data, unsigned int *next_paddr, addr_range *ar0, addr_range *ar1, unsigned int *start_addr, addr_range *dynamic_section)
{
	struct Elf32_Ehdr *imageHeader = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr *segments = (struct Elf32_Phdr*)(imageHeader->e_phoff + (unsigned) imageHeader);
	int segmentIndex;
	int foundSegmentIndex = 0;

	ar0->size = 0;
	ar1->size = 0;
	dynamic_section->size = 0;

	for (segmentIndex = 0; segmentIndex < imageHeader->e_phnum; segmentIndex++) {
		struct Elf32_Phdr *segment = &segments[segmentIndex];
		unsigned segmentOffset;

		switch(segment->p_type) {
			case PT_LOAD:
				break;
			case PT_DYNAMIC:
				dynamic_section->start = segment->p_vaddr;
				dynamic_section->size = segment->p_memsz;
			default:
				continue;
		}

//		dprintf("segment %d\n", segmentIndex);
//		dprintf("p_vaddr 0x%x p_paddr 0x%x p_filesz 0x%x p_memsz 0x%x\n",
//			segment->p_vaddr, segment->p_paddr, segment->p_filesz, segment->p_memsz);

		/* Map initialized portion */
		for (segmentOffset = 0;
			segmentOffset < ROUNDUP(segment->p_filesz, PAGE_SIZE);
			segmentOffset += PAGE_SIZE) {

			mmu_map_page(segment->p_vaddr + segmentOffset, *next_paddr);
			memcpy((void *)ROUNDOWN(segment->p_vaddr + segmentOffset, PAGE_SIZE),
				(void *)ROUNDOWN((unsigned)data + segment->p_offset + segmentOffset, PAGE_SIZE), PAGE_SIZE);
			(*next_paddr) += PAGE_SIZE;
		}

		/* Clean out the leftover part of the last page */
		if(segment->p_filesz % PAGE_SIZE > 0) {
//			dprintf("memsetting 0 to va 0x%x, size %d\n", (void*)((unsigned)segment->p_vaddr + segment->p_filesz), PAGE_SIZE  - (segment->p_filesz % PAGE_SIZE));
			memset((void*)((unsigned)segment->p_vaddr + segment->p_filesz), 0, PAGE_SIZE
				- (segment->p_filesz % PAGE_SIZE));
		}

		/* Map uninitialized portion */
		for (; segmentOffset < ROUNDUP(segment->p_memsz, PAGE_SIZE); segmentOffset += PAGE_SIZE) {
//			dprintf("mapping zero page at va 0x%x\n", segment->p_vaddr + segmentOffset);
			mmu_map_page(segment->p_vaddr + segmentOffset, *next_paddr);
			memset((void *)(segment->p_vaddr + segmentOffset), 0, PAGE_SIZE);
			(*next_paddr) += PAGE_SIZE;
		}
		switch(foundSegmentIndex) {
			case 0:
				ar0->start = segment->p_vaddr;
				ar0->size = segment->p_memsz;
				break;
			case 1:
				ar1->start = segment->p_vaddr;
				ar1->size = segment->p_memsz;
				break;
			default:
				;
		}
		foundSegmentIndex++;
	}
	*start_addr = imageHeader->e_entry;
}

// allocate a page directory and page table to facilitate mapping
// pages to the 0x80000000 - 0x80400000 region.
// also identity maps the first 8MB of memory
static int mmu_init(kernel_args *ka, unsigned int *next_paddr)
{
	int i;

	// allocate a new pgdir
	pgdir = (unsigned int *)*next_paddr;
	(*next_paddr) += PAGE_SIZE;
	ka->arch_args.phys_pgdir = (unsigned int)pgdir;

	// clear out the pgdir
	for(i = 0; i < 1024; i++)
		pgdir[i] = 0;

	// make a pagetable at this random spot
	pgtable = (unsigned int *)0x11000;

	for (i = 0; i < 1024; i++) {
		pgtable[i] = (i * 0x1000) | DEFAULT_PAGE_FLAGS;
	}

	pgdir[0] = (unsigned int)pgtable | DEFAULT_PAGE_FLAGS;

	// make another pagetable at this random spot
	pgtable = (unsigned int *)0x12000;

	for (i = 0; i < 1024; i++) {
		pgtable[i] = (i * 0x1000 + 0x400000) | DEFAULT_PAGE_FLAGS;
	}

	pgdir[1] = (unsigned int)pgtable | DEFAULT_PAGE_FLAGS;

	// Get new page table and clear it out
	pgtable = (unsigned int *)*next_paddr;
	ka->arch_args.pgtables[0] = (unsigned int)pgtable;
	ka->arch_args.num_pgtables = 1;

	(*next_paddr) += PAGE_SIZE;
	for (i = 0; i < 1024; i++)
		pgtable[i] = 0;

	// put the new page table into the page directory
	// this maps the kernel at KERNEL_BASE
	pgdir[KERNEL_BASE/(4*1024*1024)] = (unsigned int)pgtable | DEFAULT_PAGE_FLAGS;

	// switch to the new pgdir
	asm("movl %0, %%eax;"
		"movl %%eax, %%cr3;" :: "m" (pgdir) : "eax");
	// Important.  Make sure supervisor threads can fault on read only pages...
	asm("movl %%eax, %%cr0" : : "a" ((1 << 31) | (1 << 16) | (1 << 5) | 1));
		// pkx: moved the paging turn-on to here.

	return 0;
}

// can only map the 4 meg region right after KERNEL_BASE, may fix this later
// if need arises.
static void mmu_map_page(unsigned int vaddr, unsigned int paddr)
{
//	dprintf("mmu_map_page: vaddr 0x%x, paddr 0x%x\n", vaddr, paddr);
	if(vaddr < KERNEL_BASE || vaddr >= (KERNEL_BASE + 4096*1024)) {
		dprintf("mmu_map_page: asked to map invalid page!\n");
		for(;;);
	}
	paddr &= ~(PAGE_SIZE-1);
//	dprintf("paddr 0x%x @ index %d\n", paddr, (vaddr % (PAGE_SIZE * 1024)) / PAGE_SIZE);
	pgtable[(vaddr % (PAGE_SIZE * 1024)) / PAGE_SIZE] = paddr | DEFAULT_PAGE_FLAGS;
}

static int check_cpu(kernel_args *ka)
{
	unsigned int data[4];
	char str[17];

	// check the eflags register to see if the cpuid instruction exists
	if((get_eflags() & 1<<21) == 0) {
		set_eflags(get_eflags() | 1<<21);
		if((get_eflags() & 1<<21) == 0) {
			// we couldn't set the ID bit of the eflags register, this cpu is old
			return -1;
		}
	}

	// we can safely call cpuid

	// print some fun data
	cpuid(0, data);

	// build the vendor string
	memset(str, 0, sizeof(str));
	*(unsigned int *)&str[0] = data[1];
	*(unsigned int *)&str[4] = data[3];
	*(unsigned int *)&str[8] = data[2];

	// get the family, model, stepping
	cpuid(1, data);
	dprintf("CPU: family %d model %d stepping %d, string '%s'\n",
		(data[0] >> 8) & 0xf, (data[0] >> 4) & 0xf, data[0] & 0xf, str);

	// check for bits we need
	cpuid(1, data);
	if(data[3] & 1<<4) {
		ka->arch_args.supports_rdtsc = true;
	} else {
		ka->arch_args.supports_rdtsc = false;
		dprintf("CPU: does not support RDTSC, disabling high resolution timer\n");
	}

	return 0;
}

void sleep(uint64 time)
{
	uint64 start = system_time();

	while(system_time() - start <= time)
		;
}

static void sort_addr_range(addr_range *range, int count)
{
	addr_range temp_range;
	int i;
	bool done;

	do {
		done = true;
		for(i = 1; i < count; i++) {
			if(range[i].start < range[i-1].start) {
				done = false;
				memcpy(&temp_range, &range[i], sizeof(temp_range));
				memcpy(&range[i], &range[i-1], sizeof(temp_range));
				memcpy(&range[i-1], &temp_range, sizeof(temp_range));
			}
		}
	} while(!done);
}

#define outb(value,port) \
	asm("outb %%al,%%dx"::"a" (value),"d" (port))


#define inb(port) ({ \
	unsigned char _v; \
	asm volatile("inb %%dx,%%al":"=a" (_v):"d" (port)); \
	_v; \
	})

#define TIMER_CLKNUM_HZ 1193167

static void calculate_cpu_conversion_factor(void)
{
	unsigned       s_low, s_high;
	unsigned       low, high;
	unsigned long  expired;
	uint64         t1, t2;
	uint64         p1, p2, p3;
	double         r1, r2, r3;


	outb(0x34, 0x43);  /* program the timer to count down mode */
	outb(0xff, 0x40);		/* low and then high */
	outb(0xff, 0x40);

	/* quick sample */
quick_sample:
	do {
		outb(0x00, 0x43); /* latch counter value */
		s_low = inb(0x40);
		s_high = inb(0x40);
	} while(s_high!= 255);
	t1 = rdtsc();
	do {
		outb(0x00, 0x43); /* latch counter value */
		low = inb(0x40);
		high = inb(0x40);
	} while(high> 224);
	t2 = rdtsc();
	p1= t2-t1;
	r1= (double)(p1)/(double)(((s_high<<8)|s_low) - ((high<<8)|low));


	/* not so quick sample */
not_so_quick_sample:
	do {
		outb(0x00, 0x43); /* latch counter value */
		s_low = inb(0x40);
		s_high = inb(0x40);
	} while(s_high!= 255);
	t1 = rdtsc();
	do {
		outb(0x00, 0x43); /* latch counter value */
		low = inb(0x40);
		high = inb(0x40);
	} while(high> 192);
	t2 = rdtsc();
	p2= t2-t1;
	r2= (double)(p2)/(double)(((s_high<<8)|s_low) - ((high<<8)|low));
	if((r1/r2)> 1.01) {
		dprintf("Tuning loop(1)\n");
		goto quick_sample;
	}
	if((r1/r2)< 0.99) {
		dprintf("Tuning loop(1)\n");
		goto quick_sample;
	}

	/* slow sample */
	do {
		outb(0x00, 0x43); /* latch counter value */
		s_low = inb(0x40);
		s_high = inb(0x40);
	} while(s_high!= 255);
	t1 = rdtsc();
	do {
		outb(0x00, 0x43); /* latch counter value */
		low = inb(0x40);
		high = inb(0x40);
	} while(high> 128);
	t2 = rdtsc();
	p3= t2-t1;
	r3= (double)(p3)/(double)(((s_high<<8)|s_low) - ((high<<8)|low));
	if((r2/r3)> 1.01) {
		dprintf("Tuning loop(2)\n");
		goto not_so_quick_sample;
	}
	if((r2/r3)< 0.99) {
		dprintf("Tuning loop(2)\n");
		goto not_so_quick_sample;
	}

	expired = ((s_high<<8)|s_low) - ((high<<8)|low);
	p3*= TIMER_CLKNUM_HZ;

	/*
	 * cv_factor contains time in usecs per CPU cycle * 2^32
	 *
	 * The code below is a bit fancy. Originally Michael Noistering
	 * had it like:
	 *
	 *     cv_factor = ((uint64)1000000<<32) * expired / p3;
	 *
	 * whic is perfect, but unfortunately 1000000ULL<<32*expired
	 * may overflow in fast cpus with the long sampling period
	 * i put there for being as accurate as possible under
	 * vmware.
	 *
	 * The below calculation is based in that we are trying
	 * to calculate:
	 *
	 *     (C*expired)/p3 -> (C*(x0<<k + x1))/p3 ->
	 *     (C*(x0<<k))/p3 + (C*x1)/p3
	 *
	 * Now the term (C*(x0<<k))/p3 is rewritten as:
	 *
	 *     (C*(x0<<k))/p3 -> ((C*x0)/p3)<<k + reminder
	 *
	 * where reminder is:
	 *
	 *     floor((1<<k)*decimalPart((C*x0)/p3))
	 *
	 * which is approximated as:
	 *
	 *     floor((1<<k)*decimalPart(((C*x0)%p3)/p3)) ->
	 *     (((C*x0)%p3)<<k)/p3
	 *
	 * So the final expression is:
	 *
	 *     ((C*x0)/p3)<<k + (((C*x0)%p3)<<k)/p3 + (C*x1)/p3
	 *
	 * Just to make things fancier we choose k based on the input
	 * parameters (we use log2(expired)/3.)
	 *
	 * Of course, you are not expected to understand any of this.
	 */
	{
		unsigned i;
		unsigned k;
		uint64 C;
		uint64 x0;
		uint64 x1;
		uint64 a, b, c;

		/* first calculate k*/
		k= 0;
		for(i= 0; i< 32; i++) {
			if(expired & (1<<i)) {
				k= i;
			}
		}
		k/= 3;

		C = 1000000ULL<<32;
		x0= expired>> k;
		x1= expired&((1<<k)-1);

		a= ((C*x0)/p3)<<k;
		b= (((C*x0)%p3)<<k)/p3;
		c= (C*x1)/p3;
#if 0
		dprintf("a=%Ld\n", a);
		dprintf("b=%Ld\n", b);
		dprintf("c=%Ld\n", c);
		dprintf("%d %Ld\n", expired, p3);
#endif
		cv_factor= a + b + c;
#if 0
		dprintf("cvf=%Ld\n", cv_factor);
#endif
	}

	if(p3/expired/1000000000LL) {
		dprintf("CPU at %Ld.%03Ld GHz\n", p3/expired/1000000000LL, ((p3/expired)%1000000000LL)/1000000LL);
	} else {
		dprintf("CPU at %Ld.%03Ld MHz\n", p3/expired/1000000LL, ((p3/expired)%1000000LL)/1000LL);
	}
}

void clearscreen()
{
	int i;

	for(i=0; i< SCREEN_WIDTH*SCREEN_HEIGHT; i++) {
		kScreenBase[i] = 0xf20;
	}
}

static void scrup()
{
	int i;
	memcpy(kScreenBase, kScreenBase + SCREEN_WIDTH,
		SCREEN_WIDTH * SCREEN_HEIGHT * 2 - SCREEN_WIDTH * 2);
	screenOffset = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
	for(i=0; i<SCREEN_WIDTH; i++)
		kScreenBase[screenOffset + i] = 0x0720;
}

int puts(const char *str)
{
	while (*str) {
		if (*str == '\n') {
			screenOffset += SCREEN_WIDTH - (screenOffset % 80);
		} else {
			kScreenBase[screenOffset++] = 0xf00 | *str;
		}
		if (screenOffset >= SCREEN_WIDTH * SCREEN_HEIGHT)
			scrup();

		str++;
	}
	return 0;
}

int dprintf(const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	puts(temp);
	return ret;
}

