/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

#define DEFAULT_FD_TABLE_SIZE	128
#define MAX_FD_TABLE_SIZE		2048

typedef enum {
	STREAM_TYPE_ANY = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
	STREAM_TYPE_PIPE
} stream_type;

typedef enum {
	_SEEK_SET = 0,
	_SEEK_CUR,
	_SEEK_END
} seek_type;

typedef void * fs_cookie;
typedef void * file_cookie;
typedef void * dir_cookie;
typedef void * fs_vnode;

typedef struct iovec {
	void *start;
	size_t len;
} iovec;

typedef struct iovecs {
	size_t num;
	size_t total_len;
	iovec vec[0];
} iovecs;

/* macro to allocate a iovec array on the stack */
#define IOVECS(name, size) \
	uint8 _##name[sizeof(iovecs) + (size)*sizeof(iovec)]; \
	iovecs *name = (iovecs *)_##name

struct file_stat {
	vnode_id 	vnid;
	stream_type	type;
	off_t		size;
};

struct fs_calls {
	int (*fs_mount)(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid);
	int (*fs_unmount)(fs_cookie fs);
	int (*fs_sync)(fs_cookie fs);

	int (*fs_lookup)(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *id);

	int (*fs_getvnode)(fs_cookie fs, vnode_id id, fs_vnode *v, bool r);
	int (*fs_putvnode)(fs_cookie fs, fs_vnode v, bool r);
	int (*fs_removevnode)(fs_cookie fs, fs_vnode v, bool r);

	int (*fs_opendir)(fs_cookie fs, fs_vnode v, dir_cookie *cookie);
	int (*fs_closedir)(fs_cookie fs, fs_vnode v, dir_cookie cookie);
	int (*fs_rewinddir)(fs_cookie fs, fs_vnode v, dir_cookie cookie);
	int (*fs_readdir)(fs_cookie fs, fs_vnode v, dir_cookie cookie, void *buf, size_t buflen);

	int (*fs_open)(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
	int (*fs_close)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	int (*fs_freecookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	int (*fs_fsync)(fs_cookie fs, fs_vnode v);

	ssize_t (*fs_read)(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buf, off_t pos, ssize_t len);
	ssize_t (*fs_write)(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, ssize_t len);
	int (*fs_seek)(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, seek_type st);
	int (*fs_ioctl)(fs_cookie fs, fs_vnode v, file_cookie cookie, int op, void *buf, size_t len);

	int (*fs_canpage)(fs_cookie fs, fs_vnode v);
	ssize_t (*fs_readpage)(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);
	ssize_t (*fs_writepage)(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);

	int (*fs_create)(fs_cookie fs, fs_vnode dir, const char *name, void *create_args, vnode_id *new_vnid);
	int (*fs_unlink)(fs_cookie fs, fs_vnode dir, const char *name);
	int (*fs_rename)(fs_cookie fs, fs_vnode olddir, const char *oldname, fs_vnode newdir, const char *newname);

	int (*fs_mkdir)(fs_cookie fs, fs_vnode base_dir, const char *name);
	int (*fs_rmdir)(fs_cookie fs, fs_vnode base_dir, const char *name);

	int (*fs_rstat)(fs_cookie fs, fs_vnode v, struct file_stat *stat);
	int (*fs_wstat)(fs_cookie fs, fs_vnode v, struct file_stat *stat, int stat_mask);
};

int vfs_init(kernel_args *ka);
int vfs_bootstrap_all_filesystems(void);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);
void *vfs_new_ioctx(void *parent_ioctx);
int vfs_free_ioctx(void *ioctx);
int vfs_test(void);

struct rlimit;
int vfs_getrlimit(int resource, struct rlimit * rlp);
int vfs_setrlimit(int resource, const struct rlimit * rlp);

image_id vfs_load_fs_module(const char *path);

/* calls needed by fs internals */
int vfs_get_vnode(fs_id fsid, vnode_id vnid, fs_vnode *v);
int vfs_put_vnode(fs_id fsid, vnode_id vnid);
int vfs_remove_vnode(fs_id fsid, vnode_id vnid);

/* calls needed by the VM for paging */
int vfs_get_vnode_from_fd(int fd, bool kernel, void **vnode);
int vfs_get_vnode_from_path(const char *path, bool kernel, void **vnode);
int vfs_put_vnode_ptr(void *vnode);
void vfs_vnode_acquire_ref(void *vnode);
void vfs_vnode_release_ref(void *vnode);
ssize_t vfs_canpage(void *vnode);
ssize_t vfs_readpage(void *vnode, iovecs *vecs, off_t pos);
ssize_t vfs_writepage(void *vnode, iovecs *vecs, off_t pos);
void *vfs_get_cache_ptr(void *vnode);
int vfs_set_cache_ptr(void *vnode, void *cache);

/* calls kernel code should make if it's trying strange stuff */
int vfs_mount(char *path, const char *device, const char *fs_name, void *args, bool kernel);
int vfs_unmount(char *path, bool kernel);
int vfs_sync(void);
int vfs_opendir(char *path, bool kernel);
int vfs_closedir(int fd, bool kernel);
int vfs_rewinddir(int fd, bool kernel);
int vfs_readdir(int fd, void *buf, size_t len, bool kernel);
int vfs_open(char *path, int omode, bool kernel);
int vfs_open_vnid(fs_id fsid, vnode_id vnid, int omode, bool kernel);
int vfs_seek(int fd, off_t pos, seek_type seek_type, bool kernel);
ssize_t vfs_read(int fd, void *buf, off_t pos, ssize_t len, bool kernel);
ssize_t vfs_write(int fd, const void *buf, off_t pos, ssize_t len, bool kernel);
int vfs_ioctl(int fd, int op, void *buf, size_t len, bool kernel);
int vfs_close(int fd, bool kernel);
int vfs_fsync(int fd, bool kernel);
int vfs_create(char *path, void *args, bool kernel);
int vfs_unlink(char *path, bool kernel);
int vfs_rename(char *path, char *newpath, bool kernel);
int vfs_mkdir(char *path, bool kernel);
int vfs_rmdir(char *path, bool kernel);
int vfs_rstat(char *path, struct file_stat *stat, bool kernel);
int vfs_wstat(char *path, struct file_stat *stat, int stat_mask, bool kernel);

/* calls kernel code should make for file I/O */
int sys_mount(const char *path, const char *device, const char *fs_name, void *args);
int sys_unmount(const char *path);
int sys_sync(void);
int sys_opendir(const char *path);
int sys_closedir(int fd);
int sys_rewinddir(int fd);
int sys_readdir(int fd, void *buf, size_t len);
int sys_open(const char *path, int omode);
int sys_close(int fd);
int sys_fsync(int fd);
ssize_t sys_read(int fd, void *buf, off_t pos, ssize_t len);
ssize_t sys_write(int fd, const void *buf, off_t pos, ssize_t len);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_create(const char *path);
int sys_unlink(const char *path);
int sys_rename(const char *oldpath, const char *newpath);
int sys_mkdir(const char *path);
int sys_rmdir(const char *path);
int sys_rstat(const char *path, struct file_stat *stat);
int sys_wstat(const char *path, struct file_stat *stat, int stat_mask);
char *sys_getcwd(char *buf, size_t size);
int sys_setcwd(const char* path);
int sys_dup(int fd);
int sys_dup2(int ofd, int nfd);

/* calls the syscall dispatcher should use for user file I/O */
int user_mount(const char *path, const char *device, const char *fs_name, void *args);
int user_unmount(const char *path);
int user_sync(void);
int user_opendir(const char *path);
int user_closedir(int fd);
int user_rewinddir(int fd);
int user_readdir(int fd, void *buf, size_t len);
int user_open(const char *path, int omode);
int user_close(int fd);
int user_fsync(int fd);
ssize_t user_read(int fd, void *buf, off_t pos, ssize_t len);
ssize_t user_write(int fd, const void *buf, off_t pos, ssize_t len);
int user_seek(int fd, off_t pos, seek_type seek_type);
int user_ioctl(int fd, int op, void *buf, size_t len);
int user_create(const char *path);
int user_unlink(const char *path);
int user_rename(const char *oldpath, const char *newpath);
int user_mkdir(const char *path);
int user_rmdir(const char *path);
int user_rstat(const char *path, struct file_stat *stat);
int user_wstat(const char *path, struct file_stat *stat, int stat_mask);
int user_getcwd(char *buf, size_t size);
int user_setcwd(const char* path);
int user_dup(int fd);
int user_dup2(int ofd, int nfd);

#endif

