#ifndef _ARCH_PMAP_H
#define _ARCH_PMAP_H

#include <boot/stage2.h>

int arch_pmap_init(kernel_args *ka);
int arch_pmap_init2(kernel_args *ka);
int pmap_map_page(addr paddr, addr vaddr, int lock);
int pmap_unmap_page(addr vaddr);
void arch_pmap_invl_page(addr vaddr);
int pmap_get_page_mapping(addr vaddr, addr *paddr);

#endif

