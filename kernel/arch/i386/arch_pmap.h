#ifndef _I386_PMAP_H
#define _I386_PMAP_H

#include <stage2.h>

int arch_pmap_init(kernel_args *ka);
int arch_pmap_init2(kernel_args *ka);
int pmap_map_page(addr paddr, addr vaddr);
int pmap_unmap_page(addr vaddr);
void arch_pmap_invl_page(addr vaddr);

#endif

