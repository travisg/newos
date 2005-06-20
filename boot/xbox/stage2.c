/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/bootdir.h>
#include <boot/stage2.h>
#include <arch/cpu.h>
#include "stage2_priv.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <newos/elf32.h>

// stick the kernel arguments in a pseudo-random page that will be mapped
// at least during the call into the kernel. The kernel should copy the
// data out and unmap the page.
static kernel_args *ka = (kernel_args *)0x20000;

unsigned int cv_factor;

// size of bootdir in pages
static unsigned int bootdir_pages;

// working pagedir and pagetable
static unsigned int *pgdir;
static unsigned int *pgtable;

// function decls for this module
static void calculate_cpu_conversion_factor(void);
static void load_elf_image(void *data, unsigned int *next_paddr,
	addr_range *ar0, addr_range *ar1, unsigned int *start_addr, addr_range *dynamic_section);
static int mmu_init(kernel_args *ka, unsigned int *next_paddr);
static void mmu_map_page(addr_t vaddr, addr_t paddr);
static void mmu_map_large_page(addr_t vaddr, addr_t paddr);
static int check_cpu(kernel_args *ka);
static void sort_addr_range(addr_range *range, int count);

// called by the stage1 bootloader.
// State:
//   32-bit
//   mmu disabled
//   stack somewhere below 1 MB
//   supervisor mode
void stage2(void *_bootdir)
{
	unsigned int *idt;
	unsigned int *gdt;
	unsigned int next_vaddr;
	unsigned int next_paddr;
	unsigned int i;
	unsigned int kernel_entry;
	const boot_entry *bootdir = (boot_entry *)_bootdir;


	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	dprintf("stage2 bootloader entry.\n");

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

		dprintf("bootdir is %d pages long\n", bootdir_pages);
	}

	ka->bootdir_addr.start = (unsigned long)bootdir;
	ka->bootdir_addr.size = bootdir_pages * PAGE_SIZE;

	next_paddr = (unsigned int)bootdir + bootdir_pages * PAGE_SIZE;
	
	mmu_init(ka, &next_paddr);

	// load the kernel (2nd entry in the bootdir)
	load_elf_image((void *)(bootdir[1].be_offset * PAGE_SIZE + (unsigned int)bootdir), &next_paddr,
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

	dprintf("new stack at 0x%x to 0x%x\n", ka->cpu_kstack[0].start, ka->cpu_kstack[0].start + ka->cpu_kstack[0].size);

	// set up a new gdt
	{
		struct gdt_idt_descr gdt_descr;

		// find a new gdt
		gdt = (unsigned int *)next_paddr;
		ka->arch_args.phys_gdt = (unsigned int)gdt;
		next_paddr += PAGE_SIZE;

		dprintf("gdt at 0x%08x\n", ka->arch_args.phys_gdt);

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

		dprintf("gdt at virtual address 0x%08x\n", ka->arch_args.vir_gdt);

		// load 0x8 into cs and 0x10 into ds,es,fs,gs,ss
		set_cs_ds();
	}

	// set up a new idt
	{
		struct gdt_idt_descr idt_descr;

		// find a new idt
		idt = (unsigned int *)next_paddr;
		ka->arch_args.phys_idt = (unsigned int)idt;
		next_paddr += PAGE_SIZE;

		dprintf("idt at 0x%08x\n", ka->arch_args.phys_idt);

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

		dprintf("idt at virtual address 0x%08x\n", ka->arch_args.vir_idt);
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

	// map the framebuffer at 0xff800000-0xffbfffff
	mmu_map_large_page(0xff800000, 0x03c00000);
	set_fb_pointer(0xff800000);

	// mark memory that we know is used
	ka->phys_alloc_range[0].start = (unsigned int)bootdir;
	ka->phys_alloc_range[0].size = next_paddr - (unsigned int)bootdir;
	ka->num_phys_alloc_ranges = 1;

	// xbox memory map hard coded
	ka->phys_mem_range[0].start = 0;
	ka->phys_mem_range[0].size = 0x03c00000; // 64MB - 4MB of framebuffer
	ka->num_phys_mem_ranges = 1;

	// mark the bios area allocated
	// XXX do we need to?
	ka->phys_alloc_range[ka->num_phys_alloc_ranges].start = 0x9f000; // 640k - 1 page
	ka->phys_alloc_range[ka->num_phys_alloc_ranges].size = 0x61000;
	ka->num_phys_alloc_ranges++;

	// save the memory we've virtually allocated (for the kernel and other stuff)
	ka->virt_alloc_range[0].start = KERNEL_BASE;
	ka->virt_alloc_range[0].size = next_vaddr - KERNEL_BASE;
	ka->virt_alloc_range[1].start = 0xff800000;
	ka->virt_alloc_range[1].size = 4*1024*1024;	
	ka->num_virt_alloc_ranges = 2;

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

	// set up the framebuffer info structure to pass the kernel
	// this is hardcoded for the xbox
	ka->fb.enabled = 1;
	ka->fb.x_size = 640;
	ka->fb.y_size = 480;
	ka->fb.bit_depth = 32;
	ka->fb.red_mask_size = 8;
	ka->fb.red_field_position = 0;
	ka->fb.green_mask_size = 8;
	ka->fb.green_field_position = 8;
	ka->fb.blue_mask_size = 8;
	ka->fb.blue_field_position = 16;
	ka->fb.reserved_mask_size = 8;
	ka->fb.reserved_field_position = 24;
	ka->fb.mapping.start = 0xff800000;
	ka->fb.mapping.size = 4*1024*1024;
	ka->fb.phys_addr.start = 0x03c00000;
	ka->fb.phys_addr.size = 4*1024*1024;
	ka->fb.already_mapped = 1;

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

	dprintf("jumping into kernel at 0x%x\n", kernel_entry);

	ka->cons_line = get_current_console_line();;

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
// also identity maps all of physical memory
static int mmu_init(kernel_args *ka, unsigned int *next_paddr)
{
	int i;
	int meg;

	// allocate a new pgdir
	pgdir = (unsigned int *)*next_paddr;
	(*next_paddr) += PAGE_SIZE;
	ka->arch_args.phys_pgdir = (unsigned int)pgdir;

	dprintf("pgdir at %p\n", pgdir);

	// clear out the pgdir
	for(i = 0; i < 1024; i++)
		pgdir[i] = 0;

	// Identity map the first 128MB of memory temporarily
	// The kernel will toss these mappings, so we use safely unused 
	// memory (< 640KB)
	for(meg = 0; meg < 128; meg += 4) {
		// make a pagetable at this random spot
		pgtable = (unsigned int *)(0x30000 + (meg/4)*0x1000);

		for (i = 0; i < 1024; i++) {
			pgtable[i] = ((i * 0x1000) + (meg * 0x100000)) | DEFAULT_PAGE_FLAGS;
		}

		pgdir[meg/4] = (unsigned int)pgtable | DEFAULT_PAGE_FLAGS;

//		dprintf("pgtable %d at %p\n", meg/4, pgtable);
	}

	// Get a new page table to map kernel space and clear it out
	pgtable = (unsigned int *)*next_paddr;
	ka->arch_args.pgtables[0] = (unsigned int)pgtable;
	ka->arch_args.num_pgtables = 1;

	(*next_paddr) += PAGE_SIZE;
	for (i = 0; i < 1024; i++)
		pgtable[i] = 0;

	// put the new page table into the page directory
	// this maps the kernel at KERNEL_BASE
	pgdir[KERNEL_BASE/(4*1024*1024)] = (unsigned int)pgtable | DEFAULT_PAGE_FLAGS;

	dprintf("kernel pgtable at %p\n", pgtable);

	// switch to the new pgdir and turn on the paging mmu
	asm("movl %0, %%eax;"
		"movl %%eax, %%cr3;" :: "m" (pgdir) : "eax");
	// Important.  Make sure supervisor threads can fault on read only pages...
	asm("movl %%eax, %%cr0" : : "a" ((1 << 31) | (1 << 16) | (1 << 5) | 1));

	// turn on 4MB paging extensions
	asm("movl %%eax, %%cr4" : : "a" (1<<4));

	dprintf("mmu is on!\n");

	return 0;
}

// can only map the 4 meg region right after KERNEL_BASE, may fix this later
// if need arises.
static void mmu_map_page(addr_t vaddr, addr_t paddr)
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

static void mmu_map_large_page(addr_t vaddr, addr_t paddr)
{
//	dprintf("mmu_map_large_page: vaddr 0x%x, paddr 0x%x\n", vaddr, paddr);
	pgdir[vaddr / (PAGE_SIZE * 1024)] = paddr | (1<<7) | (1<<3) | DEFAULT_PAGE_FLAGS;
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

int panic(const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	puts("PANIC: ");
	puts(temp);
	puts("\n");

	puts("spinning forever...");
	for(;;);
	return ret;
}
