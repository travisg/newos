/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KTYPES_H
#define _KTYPES_H

#include <types.h>
#include <kernel/arch/ktypes.h>

typedef int thread_id;
typedef int region_id;
typedef int aspace_id;
typedef int proc_id;
typedef int sem_id;

typedef int vnode_id;
typedef int fs_id;

typedef int64 time_t;

// Handled in arch_ktypes.h

//typedef unsigned long addr;

#endif
