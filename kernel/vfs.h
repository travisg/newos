#ifndef _VFS_H
#define _VFS_H

#include <kernel.h>
#include <stage2.h>



struct fs_calls {
	int (*fs_mount)(void **fs_cookie, void *flags, fs_id id, void **priv_vnode_root);
	int (*fs_unmount)(void *fs_cookie);
	int (*fs_register_mountpoint)(void *fs_cookie, void *vnode, void *redir_vnode);
	int (*fs_unregister_mountpoint)(void *fs_cookie, void *vnode);
	int (*fs_dispose_vnode)(void *fs_cookie, void *vnode);
	int (*fs_opendir)(void *fs_cookie, void *base_vnode, const char *path, void **vnode, void **dircookie);
	int (*fs_rewinddir)(void *fs_cookie, void *dir_vnode, void *dircookie);
};

int vfs_init(kernel_args *ka);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);
void *vfs_new_ioctx();

int vfs_mount(const char *path, const char *fs_name);
int vfs_unmount(const char *path);
int vfs_opendir(const char *path, void *_base_vnode);

int vfs_helper_getnext_in_path(const char *path, int *start_pos, int *end_pos);

int vfs_test();


typedef enum {
	FILE_TYPE_FILE = 0,
	FILE_TYPE_DIR
} file_type;

#endif

