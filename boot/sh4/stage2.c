#include <boot.h>
#include <stage2.h>
#include <string.h>

#include "asm.h"
#include "serial.h"
#include "mmu.h"

#define BOOTDIR 0x8c001000
#define KERNEL_LOAD_ADDR 0xc0000000
#define PHYS_ADDR_START 0x0c000000

void switch_stacks_and_call(unsigned int stack, unsigned int call_addr, unsigned int call_arg);

int _start()
{
	unsigned int i;
	boot_entry *bootdir = (boot_entry *)BOOTDIR;
	unsigned int bootdir_len;
	unsigned int kernel_size;
	unsigned int next_vaddr;
	unsigned int next_paddr;
	kernel_args *ka;

	serial_init();
	mmu_init();

	serial_puts("Stage 2 loader entry\r\n");

	// look at the bootdir
	bootdir_len = 0;
	for(i=0; i<64; i++) {
		if(bootdir[i].be_type == BE_TYPE_NONE)
			break;
		bootdir_len += bootdir[i].be_size;
	}

	dprintf("bootdir is %d pages long\r\n", bootdir_len);
	next_paddr = PHYS_ADDR_START + bootdir_len * PAGE_SIZE;	

	// map the kernel text & data
	kernel_size = bootdir[2].be_size;	
	dprintf("kernel is %d pages long\r\n", kernel_size);		
	next_vaddr = KERNEL_LOAD_ADDR;

	for(i=0; i<kernel_size; i++) {
		unsigned int pages_mapped;
		unsigned int page_to_map = (bootdir[2].be_offset + i) * PAGE_SIZE + BOOTDIR;
		if(i + 15 < kernel_size && (page_to_map % (64*1024)) == 0 && (next_vaddr % (64*1024)) == 0) {
			mmu_map_page_64k(next_vaddr, page_to_map);
			pages_mapped = 16;
		} else {	
			mmu_map_page_4k(next_vaddr, page_to_map);
			pages_mapped = 1;
		}
		next_vaddr += PAGE_SIZE * pages_mapped;
		i += pages_mapped - 1;
	}

	dprintf("memcmp = %d\r\n", memcmp((void *)KERNEL_LOAD_ADDR, (void *)BOOTDIR + bootdir[2].be_offset * PAGE_SIZE, PAGE_SIZE));	
	
	// map in kernel bss
	// XXX assume it's 64k
	for(i=0; i<16; i++) {
		unsigned int pages_mapped;
		if(i + 15 < kernel_size && (next_paddr % (64*1024)) == 0 && (next_vaddr % (64*1024)) == 0) {
			mmu_map_page_64k(next_vaddr, next_paddr);
			pages_mapped = 16;
		} else {
			mmu_map_page_4k(next_vaddr, next_paddr);
			pages_mapped = 1;
		}			
		memset((void *)next_vaddr, 0, PAGE_SIZE * pages_mapped);
		next_vaddr += PAGE_SIZE * pages_mapped;
		next_paddr += PAGE_SIZE * pages_mapped;
		i += pages_mapped - 1;
	}

	// map in a kernel stack
	for(i=0; i<2; i++) {	
		mmu_map_page_4k(next_vaddr, next_paddr);
		next_vaddr += PAGE_SIZE;
		next_paddr += PAGE_SIZE;
	}

	// find space for a page of kernel args
	mmu_map_page_4k(next_vaddr, next_paddr);
	ka = (kernel_args *)next_vaddr;
	next_vaddr += PAGE_SIZE;
	next_paddr += PAGE_SIZE;
	
	ka->cons_line = 0;
	ka->mem_size = 16*1024*1024; // 16 MB
	ka->str = 0;
	ka->bootdir = (boot_entry *)BOOTDIR;
	ka->bootdir_size = bootdir_len;	
	ka->kernel_seg0_base = KERNEL_LOAD_ADDR;
	ka->kernel_seg0_size = kernel_size * PAGE_SIZE;
	ka->kernel_seg1_base = ka->kernel_seg0_base + ka->kernel_seg0_size;
	ka->kernel_seg1_size = 64*1024;
	ka->phys_alloc_range_low = PHYS_ADDR_START;
	ka->phys_alloc_range_high = next_paddr;
	ka->virt_alloc_range_low = KERNEL_LOAD_ADDR;
	ka->virt_alloc_range_high = next_vaddr;
	ka->num_cpus = 0;
	ka->cpu_kstack[0] = next_vaddr - 2 * PAGE_SIZE;
	ka->cpu_kstack_len[0] = 2 * PAGE_SIZE;

	dprintf("switching stack to 0x%x and calling 0x%x\n", 
		ka->cpu_kstack[0] + 2 * PAGE_SIZE - 4, KERNEL_LOAD_ADDR + 0x80);
	switch_stacks_and_call(ka->cpu_kstack[0] + 2 * PAGE_SIZE - 4, KERNEL_LOAD_ADDR + 0x80, (unsigned int)ka);
	return 0;
}

asm("
.text
.align 2
_switch_stacks_and_call:
	mov	r4,r15
	lds	r5,pr
	mov	r6,r4
	rts
	nop
");

