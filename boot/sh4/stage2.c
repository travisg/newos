/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/bootdir.h>
#include <boot/stage2.h>
#include <libc/string.h>
#include <sys/elf32.h>

#include <arch/sh4/sh4.h>
#include <arch/sh4/vcpu.h>
#include "serial.h"
#include "mmu.h"

#define BOOTDIR 0x8c001000
#define P2_AREA 0x8c000000
#define PHYS_ADDR_START 0x0c000000

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

void test_interrupt();
void switch_stacks_and_call(unsigned int stack, unsigned int call_addr, unsigned int call_arg, unsigned int call_arg2);

void load_elf_image(void *data, unsigned int *next_paddr, addr_range *ar0, addr_range *ar1, unsigned int *start_addr);

int _start()
{
	unsigned int i;
	boot_entry *bootdir = (boot_entry *)BOOTDIR;
	unsigned int bootdir_len;
	unsigned int next_vaddr;
	unsigned int next_paddr;
	unsigned int kernel_entry;
	kernel_args *ka;

	serial_init();

	serial_puts("Stage 2 loader entry\n");

	// look at the bootdir
	bootdir_len = 1; // account for bootdir directory
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
	load_elf_image((void *)(bootdir[2].be_offset * PAGE_SIZE + BOOTDIR), &next_paddr,
			&ka->kernel_seg0_addr, &ka->kernel_seg1_addr, &kernel_entry);
	dprintf("mapped kernel from 0x%x to 0x%x\n", ka->kernel_seg0_addr.start, ka->kernel_seg1_addr.start + ka->kernel_seg1_addr.size);
	dprintf("kernel entry @ 0x%x\n", kernel_entry);

#if 0
	dprintf("diffing the mapped memory\n");
	dprintf("memcmp = %d\n", memcmp((void *)KERNEL_LOAD_ADDR, (void *)BOOTDIR + bootdir[2].be_offset * PAGE_SIZE, PAGE_SIZE)); 
	dprintf("done diffing the memory\n");
#endif

	next_vaddr = ROUNDUP(ka->kernel_seg1_addr.start + ka->kernel_seg1_addr.size, PAGE_SIZE);

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
	ka->num_phys_alloc_ranges = 1;
	ka->virt_alloc_range[0].start = ka->kernel_seg0_addr.start;
	ka->virt_alloc_range[0].size  = next_vaddr - ka->virt_alloc_range[0].start;
	ka->num_virt_alloc_ranges = 1;

	ka->cons_line = 0;
	ka->str = 0;
	ka->bootdir_addr.start = P1_TO_PHYS_ADDR(BOOTDIR);
	ka->bootdir_addr.size = bootdir_len * PAGE_SIZE;	
	ka->phys_mem_range[0].start = PHYS_ADDR_START;
	ka->phys_mem_range[0].size  = 16*1024*1024;
	ka->num_phys_mem_ranges = 1;
	ka->num_cpus = 1;

	for(i=0; i<ka->num_phys_alloc_ranges; i++) {
		dprintf("prange %d start = 0x%x, size = 0x%x\n",
			i, ka->phys_alloc_range[i].start, ka->phys_alloc_range[i].size);
	}
		
	for(i=0; i<ka->num_virt_alloc_ranges; i++) {
		dprintf("vrange %d start = 0x%x, size = 0x%x\n",
			i, ka->virt_alloc_range[i].start, ka->virt_alloc_range[i].size);
	}

	dprintf("switching stack to 0x%x and calling 0x%x\n", 
		ka->cpu_kstack[0].start + ka->cpu_kstack[0].size - 4, kernel_entry);
	switch_stacks_and_call(ka->cpu_kstack[0].start + ka->cpu_kstack[0].size - 4, 
		kernel_entry, 
		(unsigned int)ka, 
		0);
	return 0;
}

asm("
.text
.align 2
_switch_stacks_and_call:
	mov	r4,r15
	mov	r5,r1
	mov	r6,r4
	jsr	@r1
	mov	r7,r5
");

void load_elf_image(void *data, unsigned int *next_paddr, addr_range *ar0, addr_range *ar1, unsigned int *start_addr)
{
	struct Elf32_Ehdr *imageHeader = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr *segments = (struct Elf32_Phdr*)(imageHeader->e_phoff + (unsigned) imageHeader);
	int segmentIndex;

	for (segmentIndex = 0; segmentIndex < imageHeader->e_phnum; segmentIndex++) {
		struct Elf32_Phdr *segment = &segments[segmentIndex];
		unsigned segmentOffset;

		/* Map initialized portion */
		for (segmentOffset = 0;
			segmentOffset < ROUNDUP(segment->p_filesz, PAGE_SIZE);
			segmentOffset += PAGE_SIZE) {

			mmu_map_page(segment->p_vaddr + segmentOffset, (unsigned) data + segment->p_offset
				+ segmentOffset);
		}

		/* Clean out the leftover part of the last page */
		if(segment->p_filesz % PAGE_SIZE > 0) {
			dprintf("memsetting 0 to va 0x%x, size %d\n", (void*)((unsigned) data + segment->p_offset + segment->p_filesz), PAGE_SIZE  - (segment->p_filesz % PAGE_SIZE));
			memset((void*)((unsigned) data + segment->p_offset + segment->p_filesz), 0, PAGE_SIZE
				- (segment->p_filesz % PAGE_SIZE));
		}

		/* Map uninitialized portion */
		for (; segmentOffset < ROUNDUP(segment->p_memsz, PAGE_SIZE); segmentOffset += PAGE_SIZE) {
			mmu_map_page(segment->p_vaddr + segmentOffset, *next_paddr);
			(*next_paddr) += PAGE_SIZE;
		}
		switch(segmentIndex) {
			case 0:
				ar0->start = segment->p_vaddr;
				ar0->size = segment->p_memsz;
				*start_addr = segment->p_vaddr;
				break;
			case 1:
				ar1->start = segment->p_vaddr;
				ar1->size = segment->p_memsz;
				break;
			default:
				;
		}
	}
}

