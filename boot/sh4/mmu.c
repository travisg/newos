#include <string.h>

#include "serial.h"
#include "mmu.h"

unsigned int next_utlb_ent = 0;

#define PAGE_SIZE_1K	0
#define PAGE_SIZE_4K	1
#define PAGE_SIZE_64K	2
#define PAGE_SIZE_1M	3

#define PTEH 	0xff000000
#define PTEL 	0xff000004
#define PTEA	0xff000034
#define TTB  	0xff000008
#define TEA  	0xff00000c
#define MMUCR 	0xff000010

#define UTLB    0xf6000000
#define UTLB1   0xf7000000
#define UTLB2   0xf3800000
#define UTLB_ADDR_SHIFT 0x8

struct utlb_addr_array {
	unsigned int asid: 8;
	unsigned int valid: 1;
	unsigned int dirty: 1;
	unsigned int vpn: 22;
};

struct utlb_data_array_1 {
	unsigned int wt: 1;
	unsigned int sh: 1;
	unsigned int dirty: 1;
	unsigned int cacheability: 1;
	unsigned int psize0: 1;
	unsigned int prot_key: 2;
	unsigned int psize1: 1;
	unsigned int valid: 1;
	unsigned int unused: 1;
	unsigned int ppn: 19;
	unsigned int unused2: 3;
};

struct utlb_data_array_2 {
	unsigned int sa: 2;
	unsigned int tc: 1;
	unsigned int unused: 29;
};

struct utlb_data {
	struct utlb_addr_array a;
	struct utlb_data_array_1 da1;
	struct utlb_data_array_2 da2;
};

#define	ITLB	0xf2000000
#define	ITLB1	0xf3000000
#define ITLB2	0xf3800000
#define ITLB_ADDR_SHIFT 0x8

struct itlb_addr_array {
	unsigned int asid: 8;
	unsigned int valid: 1;
	unsigned int unused: 1;
	unsigned int vpn: 22;
};

struct itlb_data_array_1 {
	unsigned int unused1: 1;
	unsigned int sh: 1;
	unsigned int unused2: 1;
	unsigned int cacheability: 1;
	unsigned int psize0: 1;
	unsigned int unused3: 1;
	unsigned int prot_key: 1;
	unsigned int psize1: 1;
	unsigned int valid: 1;
	unsigned int unused4: 1;
	unsigned int ppn: 19;
	unsigned int unused5: 3;
};

struct itlb_data_array_2 {
	unsigned int sa: 2;
	unsigned int tc: 1;
	unsigned int unused: 29;
};

struct itlb_data {
	struct itlb_addr_array a;
	struct itlb_data_array_1 da1;
	struct itlb_data_array_2 da2;
};

void mmu_dump_itlb_entry(int ent)
{
	struct itlb_data data;

	*(int *)&data.a = *((int *)(ITLB | (ent << ITLB_ADDR_SHIFT)));
	*(int *)&data.da1 = *((int *)(ITLB1 | (ent << ITLB_ADDR_SHIFT)));
	*(int *)&data.da2 = *((int *)(ITLB2 | (ent << ITLB_ADDR_SHIFT)));
	
	dprintf("itlb[%d] = \r\n", ent);
	dprintf(" asid = %d\r\n", data.a.asid);
	dprintf(" valid = %d\r\n", data.a.valid);
	dprintf(" vpn = 0x%x\r\n", data.a.vpn << 10);
	dprintf(" ppn = 0x%x\r\n", data.da1.ppn << 10);
}

void mmu_clear_all_itlb_entries()
{	
	int i;
	for(i=0; i<4; i++) {
		*((int *)(ITLB | (i << ITLB_ADDR_SHIFT))) = 0;
		*((int *)(ITLB1 | (i << ITLB_ADDR_SHIFT))) = 0;
		*((int *)(ITLB2 | (i << ITLB_ADDR_SHIFT))) = 0;
	}
}

void mmu_dump_all_itlb_entries()
{
	int i;

	for(i=0; i<4; i++) {
		mmu_dump_itlb_entry(i);
	}
}

void mmu_dump_utlb_entry(int ent)
{
	struct utlb_data data;

	*(int *)&data.a = *((int *)(UTLB | (ent << UTLB_ADDR_SHIFT)));
	*(int *)&data.da1 = *((int *)(UTLB1 | (ent << UTLB_ADDR_SHIFT)));
	*(int *)&data.da2 = *((int *)(UTLB2 | (ent << UTLB_ADDR_SHIFT)));
	
	dprintf("utlb[%d] = \r\n", ent);
	dprintf(" asid = %d\r\n", data.a.asid);
	dprintf(" valid = %d\r\n", data.a.valid);
	dprintf(" dirty = %d\r\n", data.a.dirty);
	dprintf(" vpn = 0x%x\r\n", data.a.vpn << 10);
	dprintf(" ppn = 0x%x\r\n", data.da1.ppn << 10);
}

void mmu_clear_all_utlb_entries()
{	
	int i;
	for(i=0; i<64; i++) {
		*((int *)(UTLB | (i << UTLB_ADDR_SHIFT))) = 0;
		*((int *)(UTLB1 | (i << UTLB_ADDR_SHIFT))) = 0;
		*((int *)(UTLB2 | (i << UTLB_ADDR_SHIFT))) = 0;
	}
}
		
void mmu_dump_all_utlb_entries()
{
	int i;

	for(i=0; i<4; i++) {
		mmu_dump_utlb_entry(i);
	}
}

void sniff_mmu()
{
	dprintf("PTEH = 0x%x\r\n", *(int *)PTEH);
	dprintf("PTEL = 0x%x\r\n", *(int *)PTEL);
	dprintf("PTEA = 0x%x\r\n", *(int *)PTEA);
	dprintf("TTB = 0x%x\r\n", *(int *)TTB);
	dprintf("TEA = 0x%x\r\n", *(int *)TEA);
	dprintf("MMUCR = 0x%x\r\n", *(int *)MMUCR);
}

static void _mmu_map_page(unsigned int vaddr, unsigned int paddr, unsigned int page_size)
{
	struct utlb_data data;

	dprintf("mmu_map_page: mapping 0x%x to 0x%x, page_size %d\r\n", paddr, vaddr, page_size);

	memset(&data, 0, sizeof(data));

	data.a.vpn = vaddr >> 10;	
	data.a.dirty = 1;
	data.a.valid = 1;
	
	data.da1.ppn = paddr >> 10;
	data.da1.psize0 = page_size & 0x1;
	data.da1.psize1 = (page_size & 0x2) ? 1 : 0; 
	data.da1.prot_key = 3;
	data.da1.valid = 1;
	data.da1.dirty = 1;
	
	if(next_utlb_ent >= 64) {
		dprintf("mmu_map_page: too many pages mapped!\n");
		for(;;);
	}

	*((int *)(UTLB | (next_utlb_ent << UTLB_ADDR_SHIFT))) = *(int *)&data.a;
	*((int *)(UTLB1 | (next_utlb_ent << UTLB_ADDR_SHIFT))) = *(int *)&data.da1;
	*((int *)(UTLB2 | (next_utlb_ent << UTLB_ADDR_SHIFT))) = *(int *)&data.da2;
	next_utlb_ent++;
}

void mmu_map_page_64k(unsigned int vaddr, unsigned int paddr)
{
	_mmu_map_page(vaddr, paddr, PAGE_SIZE_64K);
}

void mmu_map_page_4k(unsigned int vaddr, unsigned int paddr)
{
	_mmu_map_page(vaddr, paddr, PAGE_SIZE_4K);
}

void mmu_init()
{
	next_utlb_ent = 0;
	
	mmu_clear_all_itlb_entries();
	mmu_clear_all_utlb_entries();

	dprintf("enabling mmu\r\n");
	*(int *)MMUCR = 0x00000105;
}

