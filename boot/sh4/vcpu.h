#ifndef _VCPU_H
#define _VCPU_H

#include <stage2.h>
#include <vcpu_struct.h>

// can only be used in stage2
int vcpu_init(kernel_args *ka);

#endif

