#ifndef _MMU_H
#define _MMU_H

#define PAGE_SIZE 4096

void mmu_init();
void mmu_map_page_4k(unsigned int vaddr, unsigned int paddr);
void mmu_map_page_64k(unsigned int vaddr, unsigned int paddr);

#endif

