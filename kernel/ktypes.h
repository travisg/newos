#ifndef _KTYPES_H
#define _KTYPES_H

#include "arch_ktypes.h"

#define NULL ((void *)0)

typedef int bool;
#define true 1
#define false 0

typedef int thread_id;
typedef int area_id;
typedef int aspace_id;
typedef int proc_id;
typedef int sem_id;

// Handled in arch_ktypes.h

//typedef unsigned long addr;
//typedef long long time_t;

#endif
