/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _I386_FAULTS
#define _I386_FAULTS

int i386_general_protection_fault(int errorcode);
int i386_double_fault(int errorcode);

#endif

