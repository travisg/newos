/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_VM_H
#define _I386_VM_H

int i386_page_fault(int cr2reg, unsigned int fault_address);

#endif

