#ifndef _I386_FAULTS
#define _I386_FAULTS

int i386_general_protection_fault(int errorcode);
int i386_double_fault(int errorcode);

#endif

