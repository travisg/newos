/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _SYS_ERRORS_H
#define _SYS_ERRORS_H

#define NO_ERROR 0

/* General errors */
#define ERR_GENERAL              -1
#define ERR_NO_MEMORY            ERR_GENERAL-1
#define ERR_IO_ERROR             ERR_GENERAL-2
#define ERR_INVALID_ARGS         ERR_GENERAL-3
#define ERR_TIMED_OUT            ERR_GENERAL-4
#define ERR_NOT_ALLOWED          ERR_GENERAL-5
#define ERR_PERMISSION_DENIED    ERR_GENERAL-6
#define ERR_INVALID_BINARY       ERR_GENERAL-7
#define ERR_INVALID_HANDLE       ERR_GENERAL-8
#define ERR_NO_MORE_HANDLES      ERR_GENERAL-9
#define ERR_UNIMPLEMENTED        ERR_GENERAL-10
#define ERR_TOO_BIG              ERR_GENERAL-11
#define ERR_NOT_FOUND            ERR_GENERAL-12

/* Semaphore errors */
#define ERR_SEM_GENERAL          -1024
#define ERR_SEM_DELETED          ERR_SEM_GENERAL-1
#define ERR_SEM_TIMED_OUT        ERR_SEM_GENERAL-2
#define ERR_SEM_OUT_OF_SLOTS     ERR_SEM_GENERAL-3
#define ERR_SEM_NOT_ACTIVE       ERR_SEM_GENERAL-4

/* Tasker errors */
#define ERR_TASK_GENERAL         -2048
#define ERR_TASK_PROC_DELETED    ERR_TASK_GENERAL-1

/* VFS errors */
#define ERR_VFS_GENERAL          -3072
#define ERR_VFS_INVALID_FS       ERR_VFS_GENERAL-1
#define ERR_VFS_NOT_MOUNTPOINT   ERR_VFS_GENERAL-2
#define ERR_VFS_PATH_NOT_FOUND   ERR_VFS_GENERAL-3
#define ERR_VFS_INSUFFICIENT_BUF ERR_VFS_GENERAL-4
#define ERR_VFS_READONLY_FS      ERR_VFS_GENERAL-5
#define ERR_VFS_ALREADY_EXISTS   ERR_VFS_GENERAL-6

/* VM errors */
#define ERR_VM_GENERAL           -4096
#define ERR_VM_INVALID_ASPACE    ERR_VM_GENERAL-1
#define ERR_VM_INVALID_REGION    ERR_VM_GENERAL-2
#define ERR_VM_PF_FATAL          ERR_VM_GENERAL-3
#define ERR_VM_PF_BAD_ADDRESS    ERR_VM_GENERAL-4
#define ERR_VM_PF_BAD_PERM       ERR_VM_GENERAL-5
#define ERR_VM_PAGE_NOT_PRESENT  ERR_VM_GENERAL-6

#endif
