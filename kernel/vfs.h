#ifndef _VFS_H
#define _VFS_H

#include <kernel.h>
#include <stage2.h>

struct fs_calls {
	int (*fs_mount)(void **fs_cookie, void *flags, fs_id id, vnode_id *root_id);
};

int vfs_init(kernel_args *ka);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);

int vfs_mount(const char *path, const char *fs_name);


#endif

