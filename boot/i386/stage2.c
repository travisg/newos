#include <boot/bootdir.h>
#include <boot/stage2.h>
#include "stage2_priv.h"

#include <string.h>
#include <stdarg.h>
#include <printf.h>

const unsigned kBSSSize = 0x9000;
#define BOOTDIR_ADDR 0x100000
const boot_entry *bootdir = (boot_entry*)BOOTDIR_ADDR;
// stick the kernel arguments in a pseudo-random page that will be mapped
// at least during the call into the kernel. The kernel should copy the
// data out and unmap the page.
kernel_args *ka = (kernel_args *)0x20000;

// needed for message
unsigned short *kScreenBase = (unsigned short*) 0xb8000;
unsigned screenOffset = 0;
unsigned int line = 0;

unsigned int cv_factor = 0;

void calculate_cpu_conversion_factor();

void _start(unsigned int mem, char *str)
{
	unsigned int *old_pgdir;
	unsigned int *pgdir;
	unsigned int *new_pgtable;
	unsigned int bootdir_pages = 0;
	unsigned int new_stack;
	unsigned int *idt;
	unsigned int *gdt;
	unsigned int next_vpage;
	unsigned int nextAllocPage;
	unsigned int kernelSize;
	unsigned int i;
	
	// Important.  Make sure supervisor threads can fault on read only pages...
	asm("movl %%eax, %%cr0" : : "a" ((1 << 31) | (1 << 16) | (1 << 5) | 1));
	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	clearscreen();
	dprintf("stage2 bootloader entry.\n");

	calculate_cpu_conversion_factor();
	dprintf("system_time = %d %d\n", system_time());

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

	dprintf("kernel text & data mapped from 0x%x", next_vpage);
	for (i = 0; i < kernelSize; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;	// present, writable, global
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	dprintf(" to 0x%x\n", next_vpage);

	nextAllocPage = BOOTDIR_ADDR + (bootdir_pages + 3) * PAGE_SIZE;	/* skip rest of boot dir and two pts */

	/* Map and clear BSS */
	dprintf("kernel BSS mapped from 0x%x", next_vpage);
	for (; i < kernelSize + ROUNDUP(kBSSSize, PAGE_SIZE) / PAGE_SIZE; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	dprintf("to 0x%x\n", next_vpage);

	/* map in an initial kernel stack */
	for (; i < kernelSize + ROUNDUP(kBSSSize, PAGE_SIZE) / PAGE_SIZE + STACK_SIZE; i++) {
		new_pgtable[i] = nextAllocPage | DEFAULT_PAGE_FLAGS;
		nextAllocPage += PAGE_SIZE;
		next_vpage += PAGE_SIZE;
	}
	new_stack = kernelSize * PAGE_SIZE + ROUNDUP(kBSSSize, PAGE_SIZE) + 2 * PAGE_SIZE + KERNEL_BASE;

	dprintf("new stack at 0x%x to 0x%x\n", new_stack - STACK_SIZE * PAGE_SIZE, new_stack);

	// set up a new idt
	{
		struct gdt_idt_descr idt_descr;
		
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
		struct gdt_idt_descr gdt_descr;
		
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
		gdt[5] = 0x00cf9200;
		gdt[6] = 0x0000ffff; // seg 0x1b
		gdt[7] = 0x00cffa00;
		gdt[8] = 0x0000ffff; // seg 0x23
		gdt[9] = 0x00cff200;
		// gdt[10] & gdt[11] will be filled later by the kernel
	
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

	// also map it on the next vpage
	new_pgtable[(next_vpage % 0x400000) / PAGE_SIZE] = (unsigned int)pgdir | DEFAULT_PAGE_FLAGS;
	next_vpage += PAGE_SIZE;

	// put the new page table into the page directory
	// this maps the kernel at KERNEL_BASE
	pgdir[KERNEL_BASE/(4*1024*1024)] = (unsigned int)new_pgtable | DEFAULT_PAGE_FLAGS;

	// save the kernel args
	ka->arch_args.system_time_cv_factor = cv_factor;
	ka->arch_args.phys_pgdir = (unsigned int)pgdir;
	ka->arch_args.vir_pgdir = (unsigned int)next_vpage - PAGE_SIZE;
	ka->arch_args.pgtables[0] = (unsigned int)new_pgtable;
	ka->arch_args.num_pgtables = 1;
	ka->arch_args.phys_idt = (unsigned int)idt;
	ka->arch_args.vir_idt = (unsigned int)next_vpage - PAGE_SIZE * 3;
	ka->arch_args.phys_gdt = (unsigned int)gdt;
	ka->arch_args.vir_gdt = (unsigned int)next_vpage - PAGE_SIZE * 2;
	ka->phys_mem_range[0].start = 0;
	ka->phys_mem_range[0].size = mem;
	ka->num_phys_mem_ranges = 1;
	ka->str = str;
	ka->bootdir_addr.start = (unsigned long)bootdir;
	ka->bootdir_addr.size = bootdir_pages * PAGE_SIZE;
	ka->kernel_seg0_addr.start = KERNEL_BASE;
	ka->kernel_seg0_addr.size = kernelSize * PAGE_SIZE;
	ka->kernel_seg1_addr.start = KERNEL_BASE + kernelSize * PAGE_SIZE;
	ka->kernel_seg1_addr.size = ROUNDUP(kBSSSize, PAGE_SIZE);
	ka->phys_alloc_range[0].start = BOOTDIR_ADDR;
	ka->phys_alloc_range[0].size = nextAllocPage - BOOTDIR_ADDR;
	ka->num_phys_alloc_ranges = 1;
	ka->virt_alloc_range[0].start = KERNEL_BASE;
	ka->virt_alloc_range[0].size = next_vpage - KERNEL_BASE;
	ka->arch_args.page_hole = 0xffc00000;
	ka->cpu_kstack[0].start = new_stack - STACK_SIZE * PAGE_SIZE;
	ka->cpu_kstack[0].size = STACK_SIZE * PAGE_SIZE;
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
	dprintf("finding and booting other cpus...\n");
	smp_boot(ka);

	dprintf("jumping into kernel at 0x80000080\n");
	
	ka->cons_line = line;

	asm("movl	%0, %%eax;	"			// move stack out of way
		"movl	%%eax, %%esp; "
		: : "m" (new_stack - 4));
	asm("pushl  $0x0; "					// we're the BSP cpu (0)
		"pushl 	%0;	"					// kernel args
		"pushl 	$0x0;"					// dummy retval for call to main
		"pushl 	$0x80000080;	"		// this is the start address
		"ret;		"					// jump.
		: : "m" (ka));
}

long long get_time_base();
asm("
get_time_base:
	rdtsc
	ret
");

//void execute_n_instructions(int count);
asm("
.global execute_n_instructions
execute_n_instructions:
	movl	4(%esp), %ecx
	shrl	$4, %ecx		/* divide count by 16 */
.again:
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	xorl	%eax, %eax
	loop	.again
	ret
");

void system_time_setup(long a);
asm("
system_time_setup:
	/* First divide 1M * 2^32 by proc_clock */
	movl	$0x0F4240, %ecx
	movl	%ecx, %edx
	subl	%eax, %eax
	movl	4(%esp), %ebx
	divl	%ebx, %eax		/* should be 64 / 32 */
	movl	%eax, cv_factor
	ret
");

// long long system_time();
asm("
.global system_time
system_time:
	/* load 64-bit factor into %eax (low), %edx (high) */
	/* hand-assemble rdtsc -- read time stamp counter */
	rdtsc		/* time in %edx,%eax */

	pushl	%ebx
	pushl	%ecx
	movl	cv_factor, %ebx
	movl	%edx, %ecx	/* save high half */
	mull	%ebx 		/* truncate %eax, but keep %edx */
	movl	%ecx, %eax
	movl	%edx, %ecx	/* save high half of low */
	mull	%ebx /*, %eax*/
	/* now compute  [%edx, %eax] + [%ecx], propagating carry */
	subl	%ebx, %ebx	/* need zero to propagate carry */
	addl	%ecx, %eax
	adc		%ebx, %edx
	popl	%ecx
	popl	%ebx
	ret
");

void sleep(long long time)
{
	long long start = system_time();
	
	while(system_time() - start <= time)
		;
}

#define outb(value,port) \
	asm("outb %%al,%%dx"::"a" (value),"d" (port))


#define inb(port) ({ \
	unsigned char _v; \
	asm volatile("inb %%dx,%%al":"=a" (_v):"d" (port)); \
	_v; \
	})

#define TIMER_CLKNUM_HZ 1193167

void calculate_cpu_conversion_factor() 
{
	unsigned char	low, high;
	unsigned long	expired;
	long long		t1, t2;
	long long       time_base_ticks;
	double			timer_usecs;

	/* program the timer to count down mode */
    outb(0x34, 0x43);              

	outb(0xff, 0x40);		/* low and then high */
	outb(0xff, 0x40);

	t1 = get_time_base();
	
	execute_n_instructions(16*20000);

	t2 = get_time_base();

	outb(0x00, 0x43); /* latch counter value */
	low = inb(0x40);
	high = inb(0x40);

	expired = (ulong)0xffff - ((((ulong)high) << 8) + low);

	timer_usecs = (expired * 1.0) / (TIMER_CLKNUM_HZ/1000000.0);
	time_base_ticks = t2 -t1;

	system_time_setup((int)(1000000.0/(timer_usecs / time_base_ticks)));
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
*/

void clearscreen()
{
	int i;
	
	for(i=0; i< SCREEN_WIDTH*SCREEN_HEIGHT*2; i++) {
		kScreenBase[i] = 0xf20;
	}
}

/*
void nmessage(const char *str1, unsigned int a, const char *str2)
{
	message(str1);
	message("0x");
	put_uint_hex(a);
	message(str2);
}

void nmessage2(const char *str1, unsigned int a, const char *str2, unsigned int b, const char *str3)
{
	message(str1);
	message("0x");
	put_uint_hex(a);
	message(str2);
	message("0x");
	put_uint_hex(b);
	message(str3);
}
*/
static void scrup()
{
	int i;
	memcpy(kScreenBase, kScreenBase + SCREEN_WIDTH,
		SCREEN_WIDTH * SCREEN_HEIGHT * 2 - SCREEN_WIDTH * 2);
	screenOffset = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
	for(i=0; i<SCREEN_WIDTH; i++)
		kScreenBase[screenOffset + i] = 0x0720;
	line = SCREEN_HEIGHT - 1;
}

void puts(const char *str)
{
	while (*str) {
		if (*str == '\n') {
			line++;
			if(line > SCREEN_HEIGHT - 1)
				scrup();
			else
				screenOffset += SCREEN_WIDTH - (screenOffset % 80);
		} else {
			kScreenBase[screenOffset++] = 0xf00 | *str;
		}
		if (screenOffset > SCREEN_WIDTH * SCREEN_HEIGHT)
			scrup();
			
		str++;
	}	
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

