/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <boot/bootdir.h>
#include <string.h>
#include <stdio.h>
#include <newos/elf32.h>
#include <arch/cpu.h>

#include "stage2_priv.h"

void _start(int arg1, int arg2, void *openfirmware);
static void load_elf_image(void *data, addr_range *ar0, addr_range *ar1, unsigned int *start_addr, addr_range *dynamic_section);
static void print_bootdir_entry(const boot_entry *entry);

#define BOOTDIR_ADDR 0x101000
static const boot_entry *bootdir = (boot_entry*)BOOTDIR_ADDR;

static kernel_args ka = {0};

void _start(int arg1, int arg2, void *openfirmware)
{
	memset(&ka, 0, sizeof(ka));

	// initialize the openfirmware handle
	of_init(openfirmware);

	// set up the framebuffer text mode
//	s2_text_init(&ka);

	printf("Welcome to the stage2 bootloader!\n");

	printf("arg1 0x%x, arg2 0x%x, openfirmware %p\n", arg1, arg2, openfirmware);

	printf("msr = 0x%x\n", getmsr());

	// bring up the mmu
 	s2_mmu_init(&ka);

	printf("bootdir at %p\n", bootdir);
	print_bootdir_entry(&bootdir[2]);

	// boot the kernel
	unsigned int kernel_entry;
	load_elf_image((void *)(bootdir[2].be_offset * PAGE_SIZE + BOOTDIR_ADDR),
			&ka.kernel_seg0_addr, &ka.kernel_seg1_addr, &kernel_entry, &ka.kernel_dynamic_section_addr);

	printf("loaded kernel, entry at 0x%x\n", kernel_entry);

	s2_mmu_remap_pagetable(&ka);

	// calculate how big the bootdir is
	{
		int entry;
		int bootdir_pages = 0;

		for (entry = 0; entry < BOOTDIR_MAX_ENTRIES; entry++) {
			if (bootdir[entry].be_type == BE_TYPE_NONE)
				break;
			bootdir_pages += bootdir[entry].be_size;
		}

		ka.bootdir_addr.start = (unsigned long)bootdir;
		ka.bootdir_addr.size = bootdir_pages * PAGE_SIZE;
		printf("bootdir is  %d pages long\n", bootdir_pages);
	}

	// save some more boot info for the kernel
	ka.num_cpus = 1;

	// allocate a new stack for the kernel
	ka.cpu_kstack[0].start = ROUNDUP(ka.virt_alloc_range[0].start + ka.virt_alloc_range[0].size, PAGE_SIZE);
	ka.cpu_kstack[0].size = 2 * PAGE_SIZE;
	mmu_map_page(&ka, mmu_allocate_page(&ka), ka.cpu_kstack[0].start, true);
	mmu_map_page(&ka, mmu_allocate_page(&ka), ka.cpu_kstack[0].start + PAGE_SIZE, true);
	printf("new stack at 0x%lx\n", ka.cpu_kstack[0].start);

	// map in all of the exception handler pages
	ka.arch_args.exception_handlers.start = ROUNDUP(ka.virt_alloc_range[0].start + ka.virt_alloc_range[0].size, PAGE_SIZE);
	ka.arch_args.exception_handlers.size = 0x3000;
	mmu_map_page(&ka, 0x0, ka.arch_args.exception_handlers.start, true);
	mmu_map_page(&ka, PAGE_SIZE, ka.arch_args.exception_handlers.start + PAGE_SIZE, true);
	mmu_map_page(&ka, 2*PAGE_SIZE, ka.arch_args.exception_handlers.start + 2*PAGE_SIZE, true);
	printf("exception handlers at 0x%lx\n", ka.arch_args.exception_handlers.start);

	// remap the framebuffer into kernel space
	{
		unsigned long new_fb;
		unsigned long i;

		new_fb = ROUNDUP(ka.virt_alloc_range[0].start + ka.virt_alloc_range[0].size, PAGE_SIZE);

		printf("new_fb 0x%x, phys.start 0x%x, phys.size 0x%x\n", new_fb, ka.fb.phys_addr.start, ka.fb.phys_addr.size);

		for(i = 0; i < ka.fb.phys_addr.size; i += PAGE_SIZE) {
			mmu_map_page(&ka, ka.fb.phys_addr.start + i, new_fb + i, false);
		}

		s2_change_framebuffer_addr(&ka, new_fb);
		s2_mmu_remove_fb_bat_entries(&ka);
		printf("framebuffer remapped to 0x%lx\n", new_fb);
	}

	{
		unsigned int i;

		for(i=0; i<ka.num_phys_mem_ranges; i++) {
			printf("phys map %d: pa 0x%lx, len 0x%lx\n", i, ka.phys_mem_range[i].start, ka.phys_mem_range[i].size);
		}
		for(i=0; i<ka.num_phys_alloc_ranges; i++) {
			printf("phys alloc map %d: pa 0x%lx, len 0x%lx\n", i, ka.phys_alloc_range[i].start, ka.phys_alloc_range[i].size);
		}
		for(i=0; i<ka.num_virt_alloc_ranges; i++) {
			printf("virt alloc map %d: pa 0x%lx, len 0x%lx\n", i, ka.virt_alloc_range[i].start, ka.virt_alloc_range[i].size);
		}
	}

	printf("jumping into kernel...\n");

	ka.cons_line = s2_get_text_line();

	// call the kernel, and switch stacks while we're at it
	switch_stacks_and_call(ka.cpu_kstack[0].start + 2*PAGE_SIZE - 8, (void *)kernel_entry, (int)&ka, 0);

	printf("should not get here\n");
	for(;;);
}

static void print_bootdir_entry(const boot_entry *entry)
{
	printf("entry at %p\n", entry);
	printf("\tbe_name: '%s'\n", entry->be_name);
	printf("\tbe_offset: %d\n", entry->be_offset);
	printf("\tbe_type: %d\n", entry->be_type);
	printf("\tbe_size: %d\n", entry->be_size);
	printf("\tbe_vsize: %d\n", entry->be_vsize);
	printf("\tbe_code_vaddr: %d\n", entry->be_code_vaddr);
	printf("\tbe_code_ventr: %d\n", entry->be_code_ventr);
}

static void load_elf_image(void *data, addr_range *ar0, addr_range *ar1, unsigned int *start_addr, addr_range *dynamic_section)
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

		printf("segment %d\n", segmentIndex);
		printf("p_vaddr 0x%x p_paddr 0x%x p_filesz 0x%x p_memsz 0x%x\n",
			segment->p_vaddr, segment->p_paddr, segment->p_filesz, segment->p_memsz);

		/* Map initialized portion */
		for (segmentOffset = 0;
			segmentOffset < ROUNDUP(segment->p_filesz, PAGE_SIZE);
			segmentOffset += PAGE_SIZE) {

			mmu_map_page(&ka, mmu_allocate_page(&ka), segment->p_vaddr + segmentOffset, true);
			memcpy((void *)ROUNDOWN(segment->p_vaddr + segmentOffset, PAGE_SIZE),
				(void *)ROUNDOWN((unsigned)data + segment->p_offset + segmentOffset, PAGE_SIZE), PAGE_SIZE);
		}

		/* Clean out the leftover part of the last page */
		if(segment->p_filesz % PAGE_SIZE > 0) {
			printf("memsetting 0 to va 0x%x, size %d\n", (void*)((unsigned)segment->p_vaddr + segment->p_filesz), PAGE_SIZE  - (segment->p_filesz % PAGE_SIZE));
			memset((void*)((unsigned)segment->p_vaddr + segment->p_filesz), 0, PAGE_SIZE
				- (segment->p_filesz % PAGE_SIZE));
		}

		/* Map uninitialized portion */
		for (; segmentOffset < ROUNDUP(segment->p_memsz, PAGE_SIZE); segmentOffset += PAGE_SIZE) {
			printf("mapping zero page at va 0x%x\n", segment->p_vaddr + segmentOffset);
			mmu_map_page(&ka, mmu_allocate_page(&ka), segment->p_vaddr + segmentOffset, true);
			memset((void *)(segment->p_vaddr + segmentOffset), 0, PAGE_SIZE);
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

