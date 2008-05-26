/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/bootdir.h>
#include <boot/stage2.h>
#include "stage2_priv.h"
#include "vesa.h"
#include "int86.h"
#include "multiboot.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <newos/elf64.h>

// we're running out of the first 'file' contained in the bootdir, which is
// a set of binaries and data packed back to back, described by an array
// of boot_entry structures at the beginning. The load address is fixed.
#define BOOTDIR_ADDR 0x400000
static const boot_entry *bootdir = (boot_entry*)BOOTDIR_ADDR;

// stick the kernel arguments in a pseudo-random page that will be mapped
// at least during the call into the kernel. The kernel should copy the
// data out and unmap the page.
kernel_args *ka = (kernel_args *)0x20000;

// next paddr to allocate
addr_t next_paddr;

// needed for message
static unsigned short *kScreenBase = (unsigned short*) 0xb8000;
static unsigned screenOffset = 0;

unsigned int cv_factor = 0;

// size of bootdir in pages
static unsigned int bootdir_pages = 0;

// function decls for this module
static void calculate_cpu_conversion_factor(void);
static void load_elf_image(void *data,
	addr_range *ar0, addr_range *ar1, addr_t *start_addr, addr_range *dynamic_section);
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
//   long mode (64bit)
//   mmu enabled, first 16MB identity mapped
//   stack somewhere below 1 MB
//   supervisor mode
void stage2_main(void *multiboot_info, unsigned int memsize, void *extended_mem_block, unsigned int extended_mem_count)
{
	unsigned int *idt;
	unsigned int *gdt;
	addr_t kernel_base;
	addr_t next_vaddr;
	unsigned int i;
	addr_t kernel_entry;

	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	dprintf("stage2 bootloader entry.\n");

	dump_multiboot(multiboot_info);

	// calculate the conversion factor that translates rdtsc time to real microseconds
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

	next_paddr = BOOTDIR_ADDR + bootdir_pages * PAGE_SIZE;

#if 0
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
#endif

	mmu_init(ka);

	// load the kernel (1st entry in the bootdir)
	load_elf_image((void *)((uint64)bootdir[1].be_offset * PAGE_SIZE + BOOTDIR_ADDR),
			&ka->kernel_seg0_addr, &ka->kernel_seg1_addr, &kernel_entry, &ka->kernel_dynamic_section_addr);

	// find the footprint of the kernel
	if (ka->kernel_seg1_addr.size > 0)
		next_vaddr = ROUNDUP(ka->kernel_seg1_addr.start + ka->kernel_seg1_addr.size, PAGE_SIZE);
	else
		next_vaddr = ROUNDUP(ka->kernel_seg0_addr.start + ka->kernel_seg0_addr.size, PAGE_SIZE);

	if (ka->kernel_seg1_addr.size > 0) {
		kernel_base = ROUNDOWN(min(ka->kernel_seg0_addr.start, ka->kernel_seg1_addr.start), PAGE_SIZE);
	} else {
		kernel_base = ROUNDOWN(ka->kernel_seg0_addr.start, PAGE_SIZE);
	}

	// map in a kernel stack
	ka->cpu_kstack[0].start = next_vaddr;
	for (i=0; i<STACK_SIZE; i++) {
		mmu_map_page(next_vaddr, next_paddr);
		next_vaddr += PAGE_SIZE;
		next_paddr += PAGE_SIZE;
	}
	ka->cpu_kstack[0].size = next_vaddr - ka->cpu_kstack[0].start;

	dprintf("new stack at 0x%lx to 0x%lx\n", ka->cpu_kstack[0].start, ka->cpu_kstack[0].start + ka->cpu_kstack[0].size);

	// set up a new gdt
	{
		struct gdt_idt_descr gdt_descr;

		// find a new gdt
		gdt = (unsigned int *)next_paddr;
		ka->arch_args.phys_gdt = (addr_t)gdt;
		next_paddr += PAGE_SIZE;

		// put segment descriptors in it
		gdt[0] = 0;
		gdt[1] = 0;
		gdt[2] = 0x00000000; // seg 0x8  -- ring 0, 64bit code
		gdt[3] = 0x00af9a00;

		// map the gdt into virtual space
		mmu_map_page(next_vaddr, (addr_t)gdt);
		ka->arch_args.vir_gdt = (addr_t)next_vaddr;
		next_vaddr += PAGE_SIZE;

		// load the GDT
		gdt_descr.a = GDT_LIMIT - 1;
		gdt_descr.b = (unsigned int *)ka->arch_args.vir_gdt;

		asm("lgdt	%0;"
			: : "m" (gdt_descr));
	}

	// set up a new idt
	{
		struct gdt_idt_descr idt_descr;

		// find a new idt
		idt = (unsigned int *)next_paddr;
		ka->arch_args.phys_idt = (addr_t)idt;
		next_paddr += PAGE_SIZE;

		// clear it out
		for(i=0; i<IDT_LIMIT/4; i++) {
			idt[i] = 0;
		}

		// map the idt into virtual space
		mmu_map_page(next_vaddr, (addr_t)idt);
		ka->arch_args.vir_idt = (addr_t)next_vaddr;
		next_vaddr += PAGE_SIZE;

		// load the idt
		idt_descr.a = IDT_LIMIT - 1;
		idt_descr.b = (unsigned int *)ka->arch_args.vir_idt;

		asm("lidt	%0;"
			: : "m" (idt_descr));
	}

	// mark memory that we know is used
	ka->phys_alloc_range[0].start = BOOTDIR_ADDR;
	ka->phys_alloc_range[0].size = next_paddr - BOOTDIR_ADDR;
	ka->num_phys_alloc_ranges = 1;

	// figure out the memory map
	fill_ka_memranges(multiboot_info);

#if 0
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
#endif

	// save the memory we've virtually allocated (for the kernel and other stuff)
	ka->virt_alloc_range[0].start = kernel_base;
	ka->virt_alloc_range[0].size = next_vaddr - kernel_base;
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
//	smp_boot(ka, kernel_entry);

	dprintf("jumping into kernel at 0x%lx\n", kernel_entry);

	ka->cons_line = screenOffset / SCREEN_WIDTH;

	asm volatile("mov	%0, %%rsp;"
				"mov	%1, %%rdi;"
				"mov	%2, %%rsi;"
				"call	*%3"
				:: "r" (ka->cpu_kstack[0].start + ka->cpu_kstack[0].size), 
				"rdi" (ka), 
				"rsi" (0),
				"r" (kernel_entry));

	for(;;);
}

static void load_elf_image(void *data, addr_range *ar0, addr_range *ar1, addr_t *start_addr, addr_range *dynamic_section)
{
	struct Elf64_Ehdr *imageHeader = (struct Elf64_Ehdr*) data;
	struct Elf64_Phdr *segments = (struct Elf64_Phdr*)(imageHeader->e_phoff + (addr_t)imageHeader);
	int segmentIndex;
	int foundSegmentIndex = 0;

	ar0->size = 0;
	ar1->size = 0;
	dynamic_section->size = 0;

	for (segmentIndex = 0; segmentIndex < imageHeader->e_phnum; segmentIndex++) {
		struct Elf64_Phdr *segment = &segments[segmentIndex];
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

		dprintf("segment %d\n", segmentIndex);
		dprintf("p_vaddr 0x%lx p_paddr 0x%lx p_filesz 0x%lx p_memsz 0x%lx\n",
			segment->p_vaddr, segment->p_paddr, segment->p_filesz, segment->p_memsz);

		/* Map initialized portion */
		for (segmentOffset = 0;
			segmentOffset < ROUNDUP(segment->p_filesz, PAGE_SIZE);
			segmentOffset += PAGE_SIZE) {

			mmu_map_page(segment->p_vaddr + segmentOffset, next_paddr);
			memcpy((void *)ROUNDOWN(segment->p_vaddr + segmentOffset, PAGE_SIZE),
				(void *)ROUNDOWN((addr_t)data + segment->p_offset + segmentOffset, PAGE_SIZE), PAGE_SIZE);
			next_paddr += PAGE_SIZE;
		}

		/* Clean out the leftover part of the last page */
		if(segment->p_filesz % PAGE_SIZE > 0) {
			dprintf("memsetting 0 to va 0x%lx, size %d\n", (void*)(segment->p_vaddr + segment->p_filesz), PAGE_SIZE  - (segment->p_filesz % PAGE_SIZE));
			memset((void*)(segment->p_vaddr + segment->p_filesz), 0, PAGE_SIZE
				- (segment->p_filesz % PAGE_SIZE));
		}

		/* Map uninitialized portion */
		for (; segmentOffset < ROUNDUP(segment->p_memsz, PAGE_SIZE); segmentOffset += PAGE_SIZE) {
			dprintf("mapping zero page at va 0x%lx\n", segment->p_vaddr + segmentOffset);
			mmu_map_page(segment->p_vaddr + segmentOffset, next_paddr);
			memset((void *)(segment->p_vaddr + segmentOffset), 0, PAGE_SIZE);
			next_paddr += PAGE_SIZE;
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

uint64 system_time(void)
{
	return 0;
}

