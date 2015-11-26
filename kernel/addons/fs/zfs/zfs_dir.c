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
    return ERR_NOT_IMPLEMENTED;
}

int zfs_opendir(fs_cookie fs, fs_vnode v, dir_cookie *cookie)
{
    return ERR_NOT_IMPLEMENTED;
}

int zfs_closedir(fs_cookie fs, fs_vnode v, dir_cookie cookie)
{
    return ERR_NOT_IMPLEMENTED;
}

int zfs_rewinddir(fs_cookie fs, fs_vnode v, dir_cookie cookie)
{
    return ERR_NOT_IMPLEMENTED;
}

int zfs_readdir(fs_cookie fs, fs_vnode v, dir_cookie cookie, void *buf, size_t buflen)
{
    return ERR_NOT_IMPLEMENTED;
}

int zfs_mkdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
    return ERR_NOT_IMPLEMENTED;
}

int zfs_rmdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
    return ERR_NOT_IMPLEMENTED;
}

