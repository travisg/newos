#include <boot.h>
#include <stage2.h>
#include <string.h>

#include "sh4.h"
#include "serial.h"
#include "mmu.h"
#include "vcpu.h"

#define BOOTDIR 0x8c001000
#define P2_AREA 0x8c000000
#define KERNEL_LOAD_ADDR 0xc0000000
#define PHYS_ADDR_START 0x0c000000

void test_interrupt();
void switch_stacks_and_call(unsigned int stack, unsigned int call_addr, unsigned int call_arg, unsigned int call_arg2);

int _start()
{
	unsigned int i;
	boot_entry *bootdir = (boot_entry *)BOOTDIR;
	unsigned int bootdir_len;
	unsigned int next_vaddr;
	unsigned int next_paddr;
	unsigned int kernel_size;
	kernel_args *ka;

	serial_init();

	serial_puts("Stage 2 loader entry\n");

	// look at the bootdir
	bootdir_len = 0;
	for(i=0; i<64; i++) {
		if(bootdir[i].be_type == BE_TYPE_NONE)
			break;
		bootdir_len += bootdir[i].be_size;
	}

	dprintf("bootdir is %d pages long\n", bootdir_len);
	next_paddr = PHYS_ADDR_START + bootdir_len * PAGE_SIZE;	

	// find a location for the kernel args
	ka = (kernel_args *)(P2_AREA + bootdir_len * PAGE_SIZE);	
	memset(ka, 0, sizeof(kernel_args));
	next_paddr += PAGE_SIZE;

	// initialize the vcpu
	vcpu_init(ka);
	mmu_init(ka, &next_paddr);

	// map the kernel text & data
	kernel_size = bootdir[2].be_size;
	dprintf("kernel is %d pages long\n", kernel_size);
	next_vaddr = KERNEL_LOAD_ADDR;

	for(i=0; i<kernel_size; i++) {
		unsigned int page_to_map = (bootdir[2].be_offset + i) * PAGE_SIZE + BOOTDIR;
		mmu_map_page(next_vaddr, page_to_map);
		next_vaddr += PAGE_SIZE;
	}

	dprintf("diffing the mapped memory\n");
	dprintf("memcmp = %d\n", memcmp((void *)KERNEL_LOAD_ADDR, (void *)BOOTDIR + bootdir[2].be_offset * PAGE_SIZE, PAGE_SIZE)); 
	dprintf("done diffing the memory\n");

	// map in kernel bss
	// XXX assume it's 64k
	for(i=0; i<16; i++) {
		mmu_map_page(next_vaddr, next_paddr);
		memset((void *)next_vaddr, 0, PAGE_SIZE);
		next_vaddr += PAGE_SIZE;
		next_paddr += PAGE_SIZE;
	}

	// map in a kernel stack
	ka->cpu_kstack[0].start = next_vaddr;
	for(i=0; i<2; i++) {
		mmu_map_page(next_vaddr, next_paddr);
		next_vaddr += PAGE_SIZE;
		next_paddr += PAGE_SIZE;
	} 
	ka->cpu_kstack[0].size = next_vaddr - ka->cpu_kstack[0].start;

	// record this first region of allocation space
	ka->phys_alloc_range[0].start = PHYS_ADDR_START;
	ka->phys_alloc_range[0].size = next_paddr - PHYS_ADDR_START;
	ka->virt_alloc_range[0].start = KERNEL_LOAD_ADDR;
	ka->virt_alloc_range[0].size  = next_vaddr - KERNEL_LOAD_ADDR;

	ka->cons_line = 0;
	ka->str = 0;
	ka->bootdir = (boot_entry *)BOOTDIR;
	ka->bootdir_size = bootdir_len;	
	ka->kernel_seg0_base = KERNEL_LOAD_ADDR;
	ka->kernel_seg0_size = kernel_size * PAGE_SIZE;
	ka->kernel_seg1_base = ka->kernel_seg0_base + ka->kernel_seg0_size;
	ka->kernel_seg1_size = 64*1024;
	ka->phys_mem_range[0].start = PHYS_ADDR_START;
	ka->phys_mem_range[0].size  = 16*1024*1024;
	ka->num_cpus = 0;

	for(i=0; i<MAX_PHYS_ALLOC_ADDR_RANGE; i++) {
		dprintf("prange %d start = 0x%x, size = 0x%x\n",
			i, ka->phys_alloc_range[i].start, ka->phys_alloc_range[i].size);
	}
		
	for(i=0; i<MAX_VIRT_ALLOC_ADDR_RANGE; i++) {
		dprintf("vrange %d start = 0x%x, size = 0x%x\n",
			i, ka->virt_alloc_range[i].start, ka->virt_alloc_range[i].size);
	}
		
	// force an intital page write tlb
	*(int *)KERNEL_LOAD_ADDR = 4;

	dprintf("switching stack to 0x%x and calling 0x%x\n", 
		ka->cpu_kstack[0].start + ka->cpu_kstack[0].size - 4, KERNEL_LOAD_ADDR + 0xa0);
	switch_stacks_and_call(ka->cpu_kstack[0].start + ka->cpu_kstack[0].size - 4, KERNEL_LOAD_ADDR + 0xa0, (unsigned int)ka, 0);
	return 0;
}

asm("
.text
.align 2
_switch_stacks_and_call:
	mov	r4,r15
	mov	r5,r1
	mov	r6,r4
	mov	r7,r5
	jsr	@r1
	nop
");

