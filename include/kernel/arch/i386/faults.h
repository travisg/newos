#ifndef _ARCH_FAULTS
#define _ARCH_FAULTS

#include <stage2.h>

int arch_faults_init(kernel_args *ka);

int i386_general_protection_fault(int errorcode);
int i386_double_fault(int errorcode);

#endif

