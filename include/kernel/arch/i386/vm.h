#ifndef _I386_VM_H
#define _I386_VM_H

int i386_page_fault(int cr2reg, unsigned int fault_address);

#endif

