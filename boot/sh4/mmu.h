#ifndef _MMU_H
#define _MMU_H

void mmu_init();
void mmu_map_page(unsigned int vaddr, unsigned int paddr);

#endif

