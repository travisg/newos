/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <libc/string.h>
#include "stage2_priv.h"

#define PAGE_SIZE 4096

static unsigned int primary_hash(unsigned int vsid, unsigned int vaddr);
static unsigned int secondary_hash(unsigned int primary_hash);

// BAT register defs
#define BATU_BEPI_MASK	0xfffe0000
#define BATU_LEN_256M	0x1ffc
#define BATU_VS			0x2
#define BATU_VP			0x1

#define BATL_BRPN_MASK	0xfffe0000
#define BATL_WIMG_MASK	0x78
#define BATL_WT			0x40
#define BATL_CI			0x20
#define BATL_MC			0x10
#define BATL_G			0x08
#define BATL_PP_MASK	0x3
#define BATL_PP_RO		0x1
#define BATL_PP_RW		0x2

struct pte {
	// pte upper word
	unsigned int v : 1;
	unsigned int vsid : 24;
	unsigned int hash : 1;
	unsigned int api : 6;
	// pte lower word
	unsigned int ppn : 20;
	unsigned int unused : 3;
	unsigned int r : 1;
	unsigned int c : 1;
	unsigned int wimg : 4;
	unsigned int unused1 : 1;
	unsigned int pp : 2;
};
struct pteg {
	struct pte pte[8];
};
static struct pteg *ptable = 0;
static int ptable_size = 0;
static unsigned int ptable_hash_mask = 0;


static unsigned long total_ram_size = 0;

static void print_pte(struct pte *e);


static void find_phys_memory_map(kernel_args *ka)
{
	int handle;
	int i;
	struct mem_region {
		unsigned long pa;
		int len;
	} mem_regions[33];
	int mem_regions_len = 0;

	// get the physical memory map of the system
	handle = of_finddevice("/memory");
	memset(mem_regions, 0, sizeof(mem_regions));
	mem_regions_len = of_getprop(handle, "reg", mem_regions, sizeof(mem_regions[0]) * 32);
	mem_regions_len /= sizeof(struct mem_region);

	// copy these regions over to the kernel args structure
	ka->num_phys_mem_ranges = 0;
	for(i=0; i<mem_regions_len; i++) {
		if(mem_regions[i].len > 0) {
			total_ram_size += mem_regions[i].len;
			if(i > 0) {
				if(mem_regions[i].pa == ka->phys_mem_range[ka->num_phys_mem_ranges-1].start + ka->phys_mem_range[ka->num_phys_mem_ranges-1].size) {
					// this range just extends the old one
					ka->phys_mem_range[ka->num_phys_mem_ranges-1].size += mem_regions[i].len;
					break;
				}
			}
			ka->phys_mem_range[ka->num_phys_mem_ranges].start = mem_regions[i].pa;
			ka->phys_mem_range[ka->num_phys_mem_ranges].size = mem_regions[i].len;
			ka->num_phys_mem_ranges++;
			if(ka->num_phys_mem_ranges == MAX_PHYS_MEM_ADDR_RANGE) {
				printf("too many physical memory maps, increase MAX_PHYS_MEM_ADDR_RANGE\n");
				for(;;);
			}
		}
	}
	for(i=0; i<ka->num_phys_mem_ranges; i++) {
		printf("phys map %d: pa 0x%x, len %d\n", i, ka->phys_mem_range[i].start, ka->phys_mem_range[i].size);
	}
}

static void find_used_phys_memory_map(kernel_args *ka)
{
	int handle;
	int i;
	struct translation_map {
		unsigned long va;
		int len;
		unsigned long pa;
		int mode;
	} memmap[64];
	int translation_map_len = 0;

	// get the current translation map of the system,
	// to find how much memory was mapped to load the stage1 and bootdir
	handle = of_finddevice("/chosen");
	of_getprop(handle, "mmu", &handle, sizeof(handle));
	handle = of_instance_to_package(handle);
	memset(&memmap, 0, sizeof(memmap));
	translation_map_len = of_getprop(handle, "translations", memmap, sizeof(memmap));
	translation_map_len /= sizeof(struct translation_map);
	for(i=0; i<translation_map_len; i++) {
		printf("package loaded at pa 0x%x va 0x%x, len 0x%x\n", memmap[i].pa, memmap[i].va, memmap[i].len);
		if(memmap[i].va == LOAD_ADDR) {
			// we found the translation that covers the loaded package. Save this.
			ka->phys_alloc_range[0].start = memmap[i].pa;
			ka->phys_alloc_range[0].size = memmap[i].len;
			ka->num_phys_alloc_ranges = 1;
		}
	}
}

static void tlbia()
{
	unsigned long i;

	asm volatile("sync");
	for(i=0; i< 0x40000; i += 0x1000) {
		asm volatile("tlbie %0" :: "r" (i));
		asm volatile("eieio");
		asm volatile("sync");
	}
	asm volatile("tlbsync");
	asm volatile("sync");
}

#define CACHELINE 64

void syncicache(void *address, int len)
{
	int l, off;
	char *p;

	off = (unsigned int)address & (CACHELINE - 1);
	len += off;

	l = len;
	p = (char *)address - off;
	do {
		asm volatile ("dcbst 0,%0" :: "r"(p));
		p += CACHELINE;
	} while((l -= CACHELINE) > 0);
	asm volatile ("sync");
	p = (char *)address - off;
	do {
		asm volatile ("icbi 0,%0" :: "r"(p));
		p += CACHELINE;
	} while((len -= CACHELINE) > 0);
	asm volatile ("sync");
	asm volatile ("isync");
}

int s2_mmu_init(kernel_args *ka)
{
	unsigned int ibats[8];
	unsigned int dbats[8];
	int i;

	getibats(ibats);
	getdbats(dbats);

	for(i=0; i<8; i++) {
		ibats[0] = 0;
		dbats[0] = 0;
	}
	// identity map the first 512Mb of RAM
	ibats[0] = BATU_LEN_256M | BATU_VS;
	dbats[0] = BATU_LEN_256M | BATU_VS;
	ibats[1] = BATL_MC | BATL_PP_RW;
	dbats[1] = BATL_MC | BATL_PP_RW;
	ibats[2] = 0x10000000 | BATU_LEN_256M | BATU_VS;
	dbats[2] = 0x10000000 | BATU_LEN_256M | BATU_VS;
	ibats[3] = 0x10000000 | BATL_MC | BATL_PP_RW;
	dbats[3] = 0x10000000 | BATL_MC | BATL_PP_RW;

	/* map 0x90000000 down to a usable spot in memory */
	ibats[4] = 0x20000000 | BATU_LEN_256M | BATU_VS;
	dbats[4] = 0x20000000 | BATU_LEN_256M | BATU_VS;
	ibats[5] = 0x90000000 | BATL_CI | BATL_PP_RW;
	dbats[5] = 0x90000000 | BATL_CI | BATL_PP_RW;

	setibats(ibats);
	setdbats(dbats);

	tlbia();

	// XXX remove
	s2_change_framebuffer_addr(0x26008000);

	printf("msr = 0x%x\n", getmsr());

	// figure out where physical memory is and what is being used
	find_phys_memory_map(ka);
	find_used_phys_memory_map(ka);

	// allocate a page table
	{
		unsigned long top_ram = 0;

		// find the largest address of physical memory, but with a max of 512 MB,
		// so it'll be within our 512 MB BAT window
		for(i=0; i<ka->num_phys_mem_ranges; i++) {
			if(ka->phys_mem_range[i].start + ka->phys_mem_range[i].size > top_ram) {
				if(ka->phys_mem_range[i].start + ka->phys_mem_range[i].size > 512*1024*1024) {
					if(ka->phys_mem_range[i].start < 512*1024*1024) {
						top_ram = 512*1024*1024;
						break;
					}
				}
				top_ram = ka->phys_mem_range[i].start + ka->phys_mem_range[i].size;
			}
		}
		printf("top of ram (but under 512MB) is 0x%x\n", top_ram);

		// figure the size of the new pagetable, as recommended by Motorola
		if(total_ram_size <= 8*1024*1024) {
			ptable_size = 64*1024;
		} else if(total_ram_size <= 16*1024*1024) {
			ptable_size = 128*1024;
		} else if(total_ram_size <= 32*1024*1024) {
			ptable_size = 256*1024;
		} else if(total_ram_size <= 64*1024*1024) {
			ptable_size = 512*1024;
		} else if(total_ram_size <= 128*1024*1024) {
			ptable_size = 1024*1024;
		} else if(total_ram_size <= 256*1024*1024) {
			ptable_size = 2*1024*1024;
		} else if(total_ram_size <= 512*1024*1024) {
			ptable_size = 4*1024*1024;
		} else if(total_ram_size <= 1024*1024*1024) {
			ptable_size = 8*1024*1024;
		} else if(total_ram_size <= 2*1024*1024*1024) {
			ptable_size = 16*1024*1024;
		} else {
			ptable_size = 32*1024*1024;
		}

		// figure out where to put the page table
		ptable_hash_mask = (ptable_size >> 6) - 1;
		ptable = (struct pteg *)(top_ram - ptable_size);
		printf("ptable at pa 0x%x, size 0x%x\n", ptable, ptable_size);
		printf("mask = 0x%x\n", ptable_hash_mask);

		// save it's new location in the kernel args
		ka->arch_args.page_table.start = (unsigned long)ptable;
		ka->arch_args.page_table.size = ptable_size;
		ka->arch_args.page_table_mask = ptable_hash_mask;

#if 0
		{
			struct pteg *old_ptable;
			int j;

			printf("sdr1 = 0x%x\n", getsdr1());

			old_ptable = (struct pteg *)((unsigned int)getsdr1() & 0xffff0000);
			printf("old_ptable %p\n", old_ptable);
			for(i=0; i< (64*1024) >> 6 ; i++) {
				for(j=0; j< 8; j++)
					if(old_ptable[i].pte[j].v && old_ptable[i].pte[j].vsid == 0)
						print_pte(&old_ptable[i].pte[j]);
			}
		}
#endif

		printf("memsetting pagetable and performing switch\n");
		memset(ptable, 0, ptable_size);

		// set up the segment registers
		for(i=0; i<16; i++) {
			setsr(i * 0x10000000, i);
		}

		printf("done, setting sdr1\n");
		setsdr1(((unsigned int)ptable & 0xffff0000) | (ptable_hash_mask >> 10));
		tlbia();
		printf("sdr1 = 0x%x\n", getsdr1());

#if 0
		mmu_map_page(0x96008000, 0x96008000);
		mmu_map_page(0x96009000, 0x96009000);
		mmu_map_page(0x9600a000, 0x9600a000);
		mmu_map_page(0x0, 0x30000000);

		printf("testing...\n");
		printf("hello\n");
		printf("%d\n", *(int *)0x30000000);
		printf("%d\n", *(int *)0x96008000);
		printf("hello2\n");
#endif
	}
	return 0;
}

static void print_pte(struct pte *e)
{
	printf("entry %p: ", e);
	printf("v %d ", e->v);
	if(e->v) {
		printf("vsid 0x%x ", e->vsid);
		printf("hash %d ", e->hash);
		printf("api 0x%x ", e->api);

		printf("ppn 0x%x ", e->ppn);
		printf("r %d ", e->r);
		printf("c %d ", e->c);
		printf("wimg 0x%x ", e->wimg);
		printf("pp 0x%x ", e->pp);
	}
	printf("\n");
}

void mmu_map_page(unsigned long pa, unsigned long va)
{
	unsigned int hash;
	struct pteg *pteg;
	int i;
	unsigned int vsid;

	// lookup the vsid based off the va
	vsid = getsr(va) & 0xffffff;

//	printf("mmu_map_page: vsid %d, pa 0x%x, va 0x%x\n", vsid, pa, va);

	hash = primary_hash(vsid, va);
//	printf("hash = 0x%x\n", hash);

	pteg = &ptable[hash];
//	printf("pteg @ 0x%x\n", pteg);

	// search for the first free slot for this pte
	for(i=0; i<8; i++) {
//		printf("trying pteg[%i]\n", i);
		if(pteg->pte[i].v == 0) {
			// upper word
			pteg->pte[i].ppn = pa / PAGE_SIZE;
			pteg->pte[i].unused = 0;
			pteg->pte[i].r = 0;
			pteg->pte[i].c = 0;
			pteg->pte[i].wimg = 0x0;
			pteg->pte[i].unused1 = 0;
			pteg->pte[i].pp = 0x2; // RW
			asm volatile("eieio");
			// lower word
			pteg->pte[i].vsid = vsid;
			pteg->pte[i].hash = 0; // primary
			pteg->pte[i].api = (va >> 22) & 0x3f;
			pteg->pte[i].v = 1;
			tlbia();
//			printf("set pteg to ");
//			print_pte(&pteg->pte[i]);
// 			printf("set pteg to 0x%x 0x%x\n", *((int *)&pteg->pte[i]), *(((int *)&pteg->pte[i])+1));
			return;
		}
	}
}

static unsigned int primary_hash(unsigned int vsid, unsigned int vaddr)
{
	unsigned int page_index;

	vsid &= 0x7ffff;
	page_index = (vaddr >> 12) & 0xffff;

	return (vsid ^ page_index) & ptable_hash_mask;
}

static unsigned int secondary_hash(unsigned int primary_hash)
{
	return ~primary_hash;
}

