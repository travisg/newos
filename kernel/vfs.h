#ifndef _VFS_H
#define _VFS_H

#include <kernel.h>
#include <stage2.h>

struct dirent {
	int name_len;
	char name[0];
};

struct fs_calls {
	int (*fs_mount)(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **priv_vnode_root);
	int (*fs_unmount)(void *fs_cookie);
	int (*fs_register_mountpoint)(void *fs_cookie, void *vnode, void *redir_vnode);
	int (*fs_unregister_mountpoint)(void *fs_cookie, void *vnode);
	int (*fs_dispose_vnode)(void *fs_cookie, void *vnode);
	int (*fs_opendir)(void *fs_cookie, void *base_vnode, const char *path, void **fs_root_vnode, void **vnode, void **dircookie);
	int (*fs_readdir)(void *fs_cookie, void *dir_vnode, void *dircookie, void *buf, unsigned int *buf_len);
	int (*fs_rewinddir)(void *fs_cookie, void *dir_vnode, void *dircookie);
	int (*fs_closedir)(void *fs_cookie, void *dir_vnode, void *dircookie);
	int (*fs_freedircookie)(void *fs_cookie, void *vnode, void *dircookie);
	int (*fs_mkdir)(void *fs_cookie, void *base_vnode, const char *path);
	int (*fs_open)(void *fs_cookie, void *base_vnode, const char *path, void **fs_root_vnode, void **vnode, void **cookie);
	int (*fs_read)(void *fs_cookie, void *vnode, void *cookie, void *buf, off_t pos, size_t *len);
	int (*fs_write)(void *fs_cookie, void *vnode, void *cookie, const void *buf, off_t pos, size_t *len);
	int (*fs_close)(void *fs_cookie, void *vnode, void *cookie);
	int (*fs_freecookie)(void *fs_cookie, void *vnode, void *cookie);
};

int vfs_init(kernel_args *ka);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);
void *vfs_new_ioctx();

int vfs_mount(const char *path, const char *fs_name);
int vfs_unmount(const char *path);

int vfs_opendir(void *_base_vnode, const char *path);
int vfs_opendir_loopback(void *_base_vnode, const char *path, void **fs_root_vnode, void **vnode, void **dircookie);

int vfs_readdir(int fd, void *buf, unsigned int *buf_len);
int vfs_rewinddir(int fd);
int vfs_closedir(int fd);

int vfs_mkdir_loopback(void *_base_vnode, const char *path);
int vfs_mkdir(void *_base_vnode, const char *path);

int vfs_open(void *_base_vnode, const char *path);
int vfs_open_loopback(void *_base_vnode, const char *path, void **base_vnode, void **vnode, void **cookie);

int vfs_read(int fd, void *buf, off_t pos, size_t *len);
int vfs_write(int fd, const void *buf, off_t pos, size_t *len);
int vfs_close(int fd);

int vfs_helper_getnext_in_path(const char *path, int *start_pos, int *end_pos);

int vfs_test();


typedef enum {
	FILE_TYPE_FILE = 0,
	FILE_TYPE_DIR,
	FILE_TYPE_DEVICE
} file_type;

#endif

