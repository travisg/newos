/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define FUNC(name) .align 2 ; .globl name ; name:

FUNC(sys_null)

FUNC(sys_open)

FUNC(sys_seek)

FUNC(sys_read)

FUNC(sys_write)

FUNC(sys_ioctl)

FUNC(sys_close)

FUNC(sys_create)

FUNC(sys_system_time)

FUNC(sys_snooze)

FUNC(sys_sem_create)

FUNC(sys_sem_delete)

FUNC(sys_sem_acquire)

FUNC(sys_sem_acquire_etc)

FUNC(sys_sem_release)

FUNC(sys_sem_release_etc)

FUNC(sys_get_current_thread_id)


