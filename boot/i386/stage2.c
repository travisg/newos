#include "boot.h"
#include "stage2.h"

#define PAGE_SIZE 0x1000
#define KERNEL_BASE 0x80000000
#define STACK_SIZE 2
#define DEFAULT_PAGE_FLAGS (1 | 2 | 256) // present/rw/global
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24

#define _PACKED __attribute__((packed)) 	

const unsigned kBSSSize = 0x9000;
#define BOOTDIR_ADDR 0x100000
const boot_entry *bootdir = (boot_entry*)BOOTDIR_ADDR;
// stick the kernel arguments in a pseudo-random page that will be mapped
// at least during the call into the kernel. The kernel should copy the
// data out and unmap the page.
struct kernel_args *ka = (struct kernel_args *)0x20000;

// needed for message
unsigned short *kScreenBase = (unsigned short*) 0xb8000;
unsigned screenOffset = 0;
unsigned int line = 0;

void put_uint_dec(unsigned int a);
void put_uint_hex(unsigned int a);
void clearscreen();
void message(const char*);

#define nmessage(a, b, c) \
	message(a); \
	message("0x"); \
	put_uint_hex(b); \
	message(c);

#define ROUNDUP(a, b) (((a) + ((b) - 1)) & (~((b) - 1)))

void _start(unsigned int mem, char *str)
{
	unsigned int *old_pgdir;
	unsigned int *pgdir;
	unsigned int *new_pgtable;
	unsigned int bootdir_pages = 0;
	int i;
	unsigned int new_stack;
	unsigned int *idt;
	unsigned int *gdt;
	unsigned int next_vpage;
	unsigned int nextAllocPage;
	unsigned int kernelSize;
	
	// Important.  Make sure supervisor threads can fault on read only pages...
	asm("movl %%eax, %%cr0" : : "a" ((1 << 31) | (1 << 16) | (1 << 5) | 1));
	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	clearscreen();
	message("stage2 bootloader entry.\n");

	// calculate how big the bootdir is
	{
		int entry;
		for (entry = 0; entry < 64; entry++) {
			if (bootdir[entry].be_type == BE_TYPE_NONE)
				break;
		
			bootdir_pages += bootdir[entry].be_size;
		}

//		nmessage("bootdir is ", bootdir_pages, " pages long\n");
	}

	// get the current page directory
	asm("movl %%cr3, %%eax" : "=a" (old_pgdir));
	
	// copy the old pgdir to the new one
	pgdir =  (unsigned int *)(BOOTDIR_ADDR + (bootdir_pages + 1) * PAGE_SIZE);
	for(i = 0; i < 512; i++)
		pgdir[i] = old_pgdir[i];

	// clear out the top part of the pgdir
	for(; i < 1024; i++)
		pgdir[i] = 0;

	// switch to the new pgdir
	asm("movl %0, %%eax;"
		"movl %%eax, %%cr3;" :: "m" (pgdir) : "eax");

	// Get new page table and clear it out
	new_pgtable = (unsigned int *)(BOOTDIR_ADDR + (bootdir_pages + 2) * PAGE_SIZE);
	for (i = 0; i < 1024; i++)
		new_pgtable[i] = 0;

//	nmessage("pgtable at ", new_pgtable, "\n");

	// map kernel text and data
	kernelSize = bootdir[2].be_size;

	nextAllocPage = BOOTDIR_ADDR + bootdir[2].be_offset * PAGE_SIZE;
	next_vpage = KERNEL_BASE;

	nmessage("kernel text & data mapped from ", next_vpage, "");
	for (i = 0; i < kernelSize; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;	// present, writable, global
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	nmessage(" to ", next_vpage, "\n");

	nextAllocPage = BOOTDIR_ADDR + (bootdir_pages + 3) * PAGE_SIZE;	/* skip rest of boot dir and two pts */

	/* Map and clear BSS */
	nmessage("kernel BSS mapped from ", next_vpage, " ");
	for (; i < kernelSize + ROUNDUP(kBSSSize, PAGE_SIZE) / PAGE_SIZE; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	nmessage("to ", next_vpage, "\n");

	/* map in an initial kernel stack */
	for (; i < kernelSize + ROUNDUP(kBSSSize, PAGE_SIZE) / PAGE_SIZE + STACK_SIZE; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	new_stack = kernelSize * PAGE_SIZE + ROUNDUP(kBSSSize, PAGE_SIZE) + 2 * PAGE_SIZE + KERNEL_BASE;

	nmessage("new stack at ", new_stack - STACK_SIZE * PAGE_SIZE, "");
	nmessage("to ", new_stack, "\n");

	// set up a new idt
	{
		#define IDT_LIMIT 0x800
		
		struct idt_descr {
			unsigned short a;
			unsigned int *b;
		} _PACKED idt_descr;
		
		// find a new idt
		idt = (unsigned int *)nextAllocPage;
		nextAllocPage += PAGE_SIZE;
	
//		nmessage("idt at ", (unsigned int)idt, "\n");
	
		// clear it out
		for(i=0; i<IDT_LIMIT/4; i++) {
			idt[i] = 0;
		}
	
		idt_descr.a = IDT_LIMIT - 1;
		idt_descr.b = (unsigned int *)next_vpage;
		
		asm("lidt	%0;"
			: : "m" (idt_descr));

//		nmessage("idt at virtual address ", next_vpage, "\n");

		// map the idt into virtual space
		new_pgtable[(next_vpage % 0x400000) / PAGE_SIZE] = (unsigned int)idt | DEFAULT_PAGE_FLAGS;
		next_vpage += PAGE_SIZE;
	}

	// set up a new gdt
	{
		#define GDT_LIMIT 0x800
		
		struct gdt_descr {
			unsigned short a;
			unsigned int *b;
		} _PACKED gdt_descr;
		
		// find a new gdt
		gdt = (unsigned int *)nextAllocPage;
		nextAllocPage += PAGE_SIZE;
	
//		nmessage("gdt at ", (unsigned int)gdt, "\n");
	
		// put segment descriptors in it
		gdt[0] = 0;
		gdt[1] = 0;
		gdt[2] = 0x0000ffff; // seg 0x8
		gdt[3] = 0x00cf9a00;
		gdt[4] = 0x0000ffff; // seg 0x10
		gdt[5] = 0x00cf9300;
	
		gdt_descr.a = GDT_LIMIT - 1;
		gdt_descr.b = (unsigned int *)next_vpage;
		
		asm("lgdt	%0;"
			: : "m" (gdt_descr));

//		nmessage("gdt at virtual address ", next_vpage, "\n");

		// map the gdt into virtual space
		new_pgtable[(next_vpage % 0x400000) / PAGE_SIZE] = (unsigned int)gdt | DEFAULT_PAGE_FLAGS;
		next_vpage += PAGE_SIZE;
	}

	// Map the pg_dir into kernel space at 0xffc00000-0xffffffff
	pgdir[1023] = (unsigned int)pgdir | DEFAULT_PAGE_FLAGS;

	// put the new page table into the page directory
	// this maps the kernel at KERNEL_BASE
	pgdir[KERNEL_BASE/(4*1024*1024)] = (unsigned int)new_pgtable | DEFAULT_PAGE_FLAGS;

	// save the kernel args
	ka->pgdir = (unsigned int)pgdir;
	ka->pgtable1 = (unsigned int)new_pgtable;
	ka->phys_idt = (unsigned int)idt;
	ka->vir_idt = (unsigned int)next_vpage - PAGE_SIZE * 2;
	ka->phys_gdt = (unsigned int)gdt;
	ka->vir_gdt = (unsigned int)next_vpage - PAGE_SIZE;
	ka->mem_size = mem;
	ka->str = str;
	ka->bootdir = bootdir;
	ka->bootdir_size = bootdir_pages * PAGE_SIZE;
	ka->kernel_seg0_base = KERNEL_BASE;
	ka->kernel_seg0_size = kernelSize * PAGE_SIZE;
	ka->kernel_seg1_base = KERNEL_BASE + kernelSize * PAGE_SIZE;
	ka->kernel_seg1_size = ROUNDUP(kBSSSize, PAGE_SIZE);
	ka->phys_alloc_range_low = BOOTDIR_ADDR;
	ka->phys_alloc_range_high = nextAllocPage;
	ka->virt_alloc_range_low = KERNEL_BASE;
	ka->virt_alloc_range_high = next_vpage;
	ka->page_hole = 0xffc00000;
	ka->stack_start = new_stack - STACK_SIZE * PAGE_SIZE;
	ka->stack_end = new_stack;
			
	nmessage("kernel args at ", (unsigned int)ka, ":\n");
	nmessage("pgdir = ", (unsigned int)ka->pgdir, "\n");
	nmessage("pgtable1 = ", (unsigned int)ka->pgtable1, "\n");
	nmessage("phys_idt = ", (unsigned int)ka->phys_idt, "\n");
	nmessage("vir_idt = ", (unsigned int)ka->vir_idt, "\n");
	nmessage("phys_gdt = ", (unsigned int)ka->phys_gdt, "\n");
	nmessage("vir_gdt = ", (unsigned int)ka->vir_gdt, "\n");
	nmessage("mem_size = ", (unsigned int)ka->mem_size, "\n");
	nmessage("str = ", (unsigned int)ka->str, "\n");
	nmessage("bootdir = ", (unsigned int)ka->bootdir, "\n");
	nmessage("bootdir_size = ", (unsigned int)ka->bootdir_size, "\n");
	nmessage("phys_alloc_range_low = ", (unsigned int)ka->phys_alloc_range_low, "\n");
	nmessage("phys_alloc_range_high = ", (unsigned int)ka->phys_alloc_range_high, "\n");
	nmessage("virt_alloc_range_low = ", (unsigned int)ka->virt_alloc_range_low, "\n");
	nmessage("virt_alloc_range_high = ", (unsigned int)ka->virt_alloc_range_high, "\n");
	nmessage("page_hole = ", (unsigned int)ka->page_hole, "\n");

	message("\njumping into kernel at 0x80000080\n");

	ka->cons_line = line;

	asm("movl	%0, %%eax;	"			// move stack out of way
		"movl	%%eax, %%esp; "
		: : "m" (new_stack - 4));
	asm("pushl 	%0;	"					// kernel args
		"pushl 	$0;	"					// dummy retval for call to main
		"pushl 	$0x80000080;	"		// this is the start address
		"ret;		"					// jump.
		: : "m" (ka));
}

/*
void put_uint_dec(unsigned int a)
{
	char temp[16];
	int i;	
	
	temp[15] = 0;
	for(i=14; i>=0; i--) {
		temp[i] = (a % 10) + '0';
		a /= 10;
		if(a == 0) break;	
	}
	
	message(&temp[i]);
}
*/

void put_uint_hex(unsigned int a)
{
	char temp[16];
	int i;	
	
	temp[15] = 0;
	for(i=14; i>=2; i--) {
		unsigned int b = (a % 16);
		
		if(b < 10) temp[i] = b + '0';
		else temp[i] = (b - 10) + 'a';

		a /= 16;
		if(a == 0) {
			break;
		}	
	}	
	message(&temp[i]);
}

void clearscreen()
{
	int i;
	
	for(i=0; i< SCREEN_WIDTH*SCREEN_HEIGHT*2; i++) {
		kScreenBase[i] = 0xf20;
	}
}

void message(const char *str)
{
	while (*str) {
		if (*str == '\n') {
			line++;
			screenOffset += SCREEN_WIDTH - (screenOffset % 80);
		} else {
			kScreenBase[screenOffset++] = 0xf00 | *str;
		}
		if (screenOffset > SCREEN_WIDTH * SCREEN_HEIGHT)
			screenOffset = 0;
			
		str++;
	}	
}

