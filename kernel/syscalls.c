/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/syscalls.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/sem.h>
#include <kernel/port.h>
#include <kernel/vm.h>
#include <kernel/cpu.h>
#include <kernel/time.h>
#include <kernel/signal.h>
#include <sys/resource.h>

static int syscall_null(void)
{           
    return 0;
}           
            
typedef void (*syscall_func)(void);
            
struct syscall_table_entry {
    syscall_func syscall;
};          

#define SYSCALL_ENTRY(func) { ((syscall_func)(func)) }
        
const struct syscall_table_entry syscall_table[] = {
	SYSCALL_ENTRY(syscall_null),				/* 0 */
	SYSCALL_ENTRY(user_mount),
	SYSCALL_ENTRY(user_unmount),
	SYSCALL_ENTRY(user_sync),
	SYSCALL_ENTRY(user_open),
	SYSCALL_ENTRY(user_close),					/* 5 */
	SYSCALL_ENTRY(user_fsync),
	SYSCALL_ENTRY(user_read),
	SYSCALL_ENTRY(user_write),
	SYSCALL_ENTRY(user_seek),
	SYSCALL_ENTRY(user_ioctl),					/* 10 */
	SYSCALL_ENTRY(user_create),
	SYSCALL_ENTRY(user_unlink),
	SYSCALL_ENTRY(user_rename),
	SYSCALL_ENTRY(user_rstat),
	SYSCALL_ENTRY(user_wstat),					/* 15 */
	SYSCALL_ENTRY(system_time),
	SYSCALL_ENTRY(user_thread_snooze),
	SYSCALL_ENTRY(user_sem_create),
	SYSCALL_ENTRY(user_sem_delete),
	SYSCALL_ENTRY(user_sem_acquire),		/* 20 */
	SYSCALL_ENTRY(user_sem_acquire_etc),
	SYSCALL_ENTRY(user_sem_release),
	SYSCALL_ENTRY(user_sem_release_etc),
	SYSCALL_ENTRY(thread_get_current_thread_id),
	SYSCALL_ENTRY(thread_exit),					/* 25 */
	SYSCALL_ENTRY(user_proc_create_proc),
	SYSCALL_ENTRY(user_thread_wait_on_thread),
	SYSCALL_ENTRY(user_proc_wait_on_proc),
	SYSCALL_ENTRY(user_vm_create_anonymous_region),
	SYSCALL_ENTRY(user_vm_clone_region),		/* 30 */
	SYSCALL_ENTRY(user_vm_map_file),
	SYSCALL_ENTRY(user_vm_delete_region),
	SYSCALL_ENTRY(user_vm_get_region_info),
	SYSCALL_ENTRY(user_thread_create_user_thread),
	SYSCALL_ENTRY(thread_kill_thread),			/* 35 */
	SYSCALL_ENTRY(thread_suspend_thread),
	SYSCALL_ENTRY(thread_resume_thread),
	SYSCALL_ENTRY(proc_kill_proc),
	SYSCALL_ENTRY(proc_get_current_proc_id),
	SYSCALL_ENTRY(user_getcwd),					/* 40 */
	SYSCALL_ENTRY(user_setcwd),
	SYSCALL_ENTRY(user_port_create),
	SYSCALL_ENTRY(user_port_close),
	SYSCALL_ENTRY(user_port_delete),
	SYSCALL_ENTRY(user_port_find),				/* 45 */
	SYSCALL_ENTRY(user_port_get_info),
	SYSCALL_ENTRY(user_port_get_next_port_info),
	SYSCALL_ENTRY(user_port_buffer_size),
	SYSCALL_ENTRY(user_port_buffer_size_etc),
	SYSCALL_ENTRY(user_port_count),				/* 50 */
	SYSCALL_ENTRY(user_port_read),
	SYSCALL_ENTRY(user_port_read_etc),
	SYSCALL_ENTRY(user_port_set_owner),
	SYSCALL_ENTRY(user_port_write),
	SYSCALL_ENTRY(user_port_write_etc),			/* 55 */
	SYSCALL_ENTRY(user_sem_get_count),
	SYSCALL_ENTRY(user_sem_get_sem_info),
	SYSCALL_ENTRY(user_sem_get_next_sem_info),
	SYSCALL_ENTRY(user_set_sem_owner),
	SYSCALL_ENTRY(user_dup),					/* 60 */
	SYSCALL_ENTRY(user_dup2),
	SYSCALL_ENTRY(user_getrlimit),
	SYSCALL_ENTRY(user_setrlimit),
	SYSCALL_ENTRY(user_atomic_add),
	SYSCALL_ENTRY(user_atomic_and),				/* 65 */
	SYSCALL_ENTRY(user_atomic_or),
	SYSCALL_ENTRY(user_atomic_set),
	SYSCALL_ENTRY(user_test_and_set),
	SYSCALL_ENTRY(user_thread_get_thread_info),
	SYSCALL_ENTRY(user_thread_get_next_thread_info), /* 70 */
	SYSCALL_ENTRY(user_proc_get_proc_info),
	SYSCALL_ENTRY(user_proc_get_next_proc_info),
	SYSCALL_ENTRY(user_thread_set_priority),
	SYSCALL_ENTRY(user_opendir),
	SYSCALL_ENTRY(user_closedir),				/* 75 */
	SYSCALL_ENTRY(user_rewinddir),
	SYSCALL_ENTRY(user_readdir),
	SYSCALL_ENTRY(user_mkdir),
	SYSCALL_ENTRY(user_rmdir),
	SYSCALL_ENTRY(user_vm_get_vm_info),			/* 80 */
	SYSCALL_ENTRY(arch_restore_signal_frame),
	SYSCALL_ENTRY(user_sigaction),
	SYSCALL_ENTRY(user_send_signal),
	SYSCALL_ENTRY(user_send_proc_signal),
	SYSCALL_ENTRY(user_set_alarm),				/* 85 */
	SYSCALL_ENTRY(setpgid),
	SYSCALL_ENTRY(getpgid),
	SYSCALL_ENTRY(setsid),
};

int num_syscall_table_entries = sizeof(syscall_table) / sizeof(struct syscall_table_entry);

