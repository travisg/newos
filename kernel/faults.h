#ifndef _FAULTS_H
#define _FAULTS_H

#include "stage2.h"

int faults_init(struct kernel_args *ka);
void general_protection_fault(int errorcode);

#endif

