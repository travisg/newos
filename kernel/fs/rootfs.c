#include <kernel.h>
#include <vfs.h>
#include <debug.h>

#include "rootfs.h"

int rootfs_mount(void **fs_cookie, void *flags, fs_id id, vnode_id *root_id)
{
	TOUCH(fs_cookie);
	TOUCH(flags);
	TOUCH(id);
	TOUCH(root_id);

	dprintf("rootfs_mount: entry\n");

	return 0;
}

struct fs_calls rootfs_calls = {
	&rootfs_mount
};

// XXX
int bootstrap_rootfs()
{
	dprintf("bootstrap_rootfs: entry\n");
	
	return vfs_register_filesystem("rootfs", &rootfs_calls);
}
