/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/debug.h>

#include <string.h>

#include "zfs.h"

int zfs_lookup(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *id)
{
	return ERR_NOT_IMPLEMENTED_YET;
}

