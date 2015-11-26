/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/debug.h>

#include <string.h>

#include "fat.h"

#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#define DEBUG_MSG_PREFIX "FAT_DIR -- "

#include <kernel/debug_ext.h>

int fat_lookup(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *id)
{
    SHOW_FLOW(3, "fs %p, dir %p name '%s'", fs, dir, name);

    return ERR_NOT_IMPLEMENTED;
}

int fat_opendir(fs_cookie fs, fs_vnode v, dir_cookie *cookie)
{
    SHOW_FLOW(3, "fs %p, dir %p", fs, v);

    return ERR_NOT_IMPLEMENTED;
}

int fat_closedir(fs_cookie fs, fs_vnode v, dir_cookie cookie)
{
    SHOW_FLOW(3, "fs %p, dir %p", fs, v);

    return ERR_NOT_IMPLEMENTED;
}

int fat_rewinddir(fs_cookie fs, fs_vnode v, dir_cookie cookie)
{
    SHOW_FLOW(3, "fs %p, dir %p", fs, v);

    return ERR_NOT_IMPLEMENTED;
}

int fat_readdir(fs_cookie fs, fs_vnode v, dir_cookie cookie, void *buf, size_t buflen)
{
    SHOW_FLOW(3, "fs %p, dir %p, buf %p, len %ld", fs, v, buf, buflen);

    return ERR_NOT_IMPLEMENTED;
}

int fat_mkdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
    SHOW_FLOW(3, "fs %p, dir %p, name '%s'", _fs, _base_dir, name);

    return ERR_NOT_IMPLEMENTED;
}

int fat_rmdir(fs_cookie _fs, fs_vnode _base_dir, const char *name)
{
    SHOW_FLOW(3, "fs %p, dir %p, name '%s'", _fs, _base_dir, name);

    return ERR_NOT_IMPLEMENTED;
}

