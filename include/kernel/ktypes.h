#ifndef _KTYPES_H
#define _KTYPES_H

#include <types.h>
#include <arch_ktypes.h>

typedef int thread_id;
typedef int area_id;
typedef int aspace_id;
typedef int proc_id;
typedef int sem_id;

typedef int vnode_id;
typedef int fs_id;

typedef int64 time_t;

// Handled in arch_ktypes.h

//typedef unsigned long addr;

#endif
