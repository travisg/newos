/*
** Copyright 2002-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NFS_H
#define _NFS_H

#include <kernel/vfs.h>
#include <newos/net.h>
#include <kernel/net/socket.h>

#include "rpc.h"
#include "nfs_fs.h"

/* fs structure */
typedef struct nfs_fs {
    fs_id id;
    mutex lock;

    void *handle_hash;

    struct nfs_vnode *root_vnode;

    rpc_state rpc;
    netaddr server_addr;
    int mount_port;
    int nfs_port;

    char server_path[MNTPATHLEN];
} nfs_fs;

/* vnode structure */
typedef struct nfs_vnode {
    struct nfs_vnode *hash_next; // next in the per mount vnode table
    nfs_fs *fs;
    mutex lock;
    stream_type st;
    nfs_fhandle nfs_handle;
} nfs_vnode;

typedef struct nfs_cookie {
    nfs_vnode *v;
    union {
        struct nfs_dircookie {
            unsigned int nfscookie;
            bool at_end;
        } dir;
        struct nfs_filecookie {
            off_t pos;
            int oflags;
        } file;
    } u;
} nfs_cookie;

#define VNODETOVNID(vno) ((vnode_id)((addr_t)(vno)))
#define VNIDTOVNODE(vnid) ((nfs_vnode *)((addr_t)(vnid)))

/* fs calls */
int nfs_mount(fs_cookie *fs, fs_id id, const char *device, void *args, vnode_id *root_vnid);
int nfs_unmount(fs_cookie fs);
int nfs_sync(fs_cookie fs);

int nfs_lookup(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *id);

int nfs_getvnode(fs_cookie fs, vnode_id id, fs_vnode *v, bool r);
int nfs_putvnode(fs_cookie fs, fs_vnode v, bool r);
int nfs_removevnode(fs_cookie fs, fs_vnode v, bool r);

int nfs_open(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
int nfs_close(fs_cookie fs, fs_vnode v, file_cookie cookie);
int nfs_freecookie(fs_cookie fs, fs_vnode v, file_cookie cookie);
int nfs_fsync(fs_cookie fs, fs_vnode v);

ssize_t nfs_read(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buf, off_t pos, ssize_t len);
ssize_t nfs_write(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, ssize_t len);
int nfs_seek(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, seek_type st);
int nfs_ioctl(fs_cookie fs, fs_vnode v, file_cookie cookie, int op, void *buf, size_t len);

int nfs_canpage(fs_cookie fs, fs_vnode v);
ssize_t nfs_readpage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);
ssize_t nfs_writepage(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);

int nfs_create(fs_cookie fs, fs_vnode dir, const char *name, void *create_args, vnode_id *new_vnid);
int nfs_unlink(fs_cookie fs, fs_vnode dir, const char *name);
int nfs_rename(fs_cookie fs, fs_vnode olddir, const char *oldname, fs_vnode newdir, const char *newname);

int nfs_mkdir(fs_cookie _fs, fs_vnode _base_dir, const char *name);
int nfs_rmdir(fs_cookie _fs, fs_vnode _base_dir, const char *name);

int nfs_rstat(fs_cookie fs, fs_vnode v, struct file_stat *stat);
int nfs_wstat(fs_cookie fs, fs_vnode v, struct file_stat *stat, int stat_mask);

#endif
