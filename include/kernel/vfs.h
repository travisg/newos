/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VFS_H
#define _VFS_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

typedef enum {
	STREAM_TYPE_NULL = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
	STREAM_TYPE_STRING
} stream_type;

typedef enum {
	SEEK_SET = 0,
	SEEK_CUR,
	SEEK_END
} seek_type;

struct redir_struct {
	bool redir;
	void *vnode;
	const char *path;
};

struct vnode_stat {
	off_t size;
};

struct fs_calls {
	int (*fs_mount)(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **priv_vnode_root);
	int (*fs_unmount)(void *fs_cookie);
	int (*fs_register_mountpoint)(void *fs_cookie, void *vnode, void *redir_vnode);
	int (*fs_unregister_mountpoint)(void *fs_cookie, void *vnode);
	int (*fs_dispose_vnode)(void *fs_cookie, void *vnode);
	int (*fs_open)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, void **vnode, void **cookie, struct redir_struct *redir);
	int (*fs_seek)(void *fs_cookie, void *vnode, void *cookie, off_t pos, seek_type seek_type);
	int (*fs_read)(void *fs_cookie, void *vnode, void *cookie, void *buf, off_t pos, size_t *len);
	int (*fs_write)(void *fs_cookie, void *vnode, void *cookie, const void *buf, off_t pos, size_t *len);
	int (*fs_ioctl)(void *fs_cookie, void *vnode, void *cookie, int op, void *buf, size_t len);
	int (*fs_close)(void *fs_cookie, void *vnode, void *cookie);
	int (*fs_create)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir);
	int (*fs_stat)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir);
};

int vfs_init(kernel_args *ka);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);
void *vfs_new_ioctx();
int vfs_free_ioctx(void *ioctx);
int vfs_helper_getnext_in_path(const char *path, int *start_pos, int *end_pos);
int vfs_test();

int sys_mount(const char *path, const char *fs_name);
int sys_unmount(const char *path);

int sys_open(const char *path, const char *stream, stream_type stream_type);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_read(int fd, void *buf, off_t pos, size_t *len);
int sys_write(int fd, const void *buf, off_t pos, size_t *len);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_close(int fd);
int sys_create(const char *path, const char *stream, stream_type stream_type);
int sys_stat(const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat);

int user_open(const char *path, const char *stream, stream_type stream_type);
int user_seek(int fd, off_t pos, seek_type seek_type);
int user_read(int fd, void *buf, off_t pos, size_t *len);
int user_write(int fd, const void *buf, off_t pos, size_t *len);
int user_ioctl(int fd, int op, void *buf, size_t len);
int user_close(int fd);
int user_create(const char *path, const char *stream, stream_type stream_type);
int user_stat(const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat);

#endif

