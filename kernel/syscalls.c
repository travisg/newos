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
#include <kernel/arch/cpu.h>

#define INT32TOINT64(x, y) ((int64)(x) | ((int64)(y) << 32))

int syscall_dispatcher(unsigned long call_num, unsigned long arg0, unsigned long arg1,
	unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, uint64 *call_ret)
{
//	dprintf("syscall_dispatcher: call 0x%x, arg0 0x%x, arg1 0x%x arg2 0x%x arg3 0x%x arg4 0x%x\n",
//		call_num, arg0, arg1, arg2, arg3, arg4);

	switch(call_num) {
		case SYSCALL_NULL:
			*call_ret = 0;
			break;
		case SYSCALL_OPEN:
			*call_ret = user_open((const char *)arg0, (const char *)arg1, (stream_type)arg2);
			break;
		case SYSCALL_SEEK:
			*call_ret = user_seek((int)arg0, (off_t)INT32TOINT64(arg1, arg2), (seek_type)arg3);
			break;
		case SYSCALL_READ:
			*call_ret = user_read((int)arg0, (void *)arg1, (off_t)INT32TOINT64(arg2, arg3), (size_t *)arg4);
			break;
		case SYSCALL_WRITE:
			*call_ret = user_write((int)arg0, (const void *)arg1, (off_t)INT32TOINT64(arg2, arg3), (size_t *)arg4);
			break;
		case SYSCALL_IOCTL:
			*call_ret = user_ioctl((int)arg0, (int)arg1, (void *)arg2, (size_t)arg3);
			break;
		case SYSCALL_CLOSE:
			*call_ret = user_close((int)arg0);
			break;
		case SYSCALL_CREATE:
			*call_ret = user_create((const char *)arg0, (const char *)arg1, (stream_type)arg2);
			break;
		case SYSCALL_STAT:
			*call_ret = user_stat((const char *)arg0, (const char *)arg1, (stream_type)arg2, (struct vnode_stat *)arg3);
			break;
		case SYSCALL_SYSTEM_TIME:
			*call_ret = system_time();
			break;
		case SYSCALL_SNOOZE:
			thread_snooze((time_t)INT32TOINT64(arg0, arg1));
			*call_ret = 0;
			break;
		case SYSCALL_SEM_CREATE:
			*call_ret = sem_create((int)arg0, (const char *)arg1);
			break;
		case SYSCALL_SEM_DELETE:
			*call_ret = sem_delete((sem_id)arg0);
			break;
		case SYSCALL_SEM_ACQUIRE:
			*call_ret = sem_acquire((sem_id)arg0, (int)arg1);
			break;
		case SYSCALL_SEM_ACQUIRE_ETC: {
			int retcode;
			*call_ret = sem_acquire_etc((sem_id)arg0, (int)arg1, (int)arg2, (time_t)INT32TOINT64(arg3, arg4), &retcode);
			if(arg5 != 0) {
				// XXX protect the copy
				*(int *)arg5 = retcode;
			}
			break;
		}
		case SYSCALL_SEM_RELEASE:
			*call_ret = sem_release((sem_id)arg0, (int)arg1);
			break;
		case SYSCALL_SEM_RELEASE_ETC:
			*call_ret = sem_release_etc((sem_id)arg0, (int)arg1, (int)arg2);
			break;
		case SYSCALL_GET_CURRENT_THREAD_ID:
			*call_ret = thread_get_current_thread_id();
			break;
		case SYSCALL_EXIT_THREAD:
			thread_exit((int)arg0);
			*call_ret = 0;
			break;
		case SYSCALL_PROC_CREATE_PROC:
			*call_ret = proc_create_proc((const char *)arg0, (const char *)arg1, (int)arg2);
			break;
		case SYSCALL_THREAD_WAIT_ON_THREAD: {
			int retcode;
			*call_ret = thread_wait_on_thread((thread_id)arg0, &retcode);
			if(arg1 != 0) {
				// XXX protect the copy
				*(int *)arg1 = retcode;
			}
			break;
		}
		case SYSCALL_PROC_WAIT_ON_PROC: {
			int retcode;
			*call_ret = proc_wait_on_proc((proc_id)arg0, &retcode);
			if(arg1 != 0) {
				// XXX protect the copy
				*(int *)arg1 = retcode;
			}
			break;
		}
		case SYSCALL_VM_CREATE_ANONYMOUS_REGION:
			*call_ret = vm_create_anonymous_region(vm_get_current_user_aspace_id(),
				(char *)arg0, (void **)arg1, (int)arg2,
				(addr)arg3, (int)arg4, (int)arg5);
			break;
		case SYSCALL_VM_CLONE_REGION:
			*call_ret = vm_clone_region(vm_get_current_user_aspace_id(),
				(char *)arg0, (void **)arg1, (int)arg2,
				(region_id)arg3, (int)arg4);
			break;
		case SYSCALL_VM_MMAP_FILE:
			// XXX unimplemented
			*call_ret = -1;
			break;
		case SYSCALL_VM_DELETE_REGION:
			*call_ret = vm_delete_region(vm_get_current_user_aspace_id(), (region_id)arg0);
			break;
		case SYSCALL_VM_FIND_REGION_BY_NAME:
			*call_ret = vm_find_region_by_name(vm_get_current_user_aspace_id(), (const char *)arg0);
			break;
		case SYSCALL_VM_GET_REGION_INFO:
			*call_ret = vm_get_region_info((region_id)arg0, (vm_region_info *)arg1);
			break;
		case SYSCALL_THREAD_CREATE_THREAD:
			*call_ret = thread_create_user_thread((char *)arg0, thread_get_current_thread()->proc->id, (int)arg1, (addr)arg2);
			break;
		case SYSCALL_THREAD_KILL_THREAD:
			*call_ret = thread_kill_thread((thread_id)arg0);
			break;
		case SYSCALL_THREAD_SUSPEND_THREAD:
			*call_ret = thread_suspend_thread((thread_id)arg0);
			break;
		case SYSCALL_THREAD_RESUME_THREAD:
			*call_ret = thread_resume_thread((thread_id)arg0);
			break;
		case SYSCALL_PROC_KILL_PROC:
			*call_ret = proc_kill_proc((proc_id)arg0);
			break;
		case SYSCALL_GET_CURRENT_PROC_ID:
			*call_ret = proc_get_current_proc_id();
			break;
		default:
			*call_ret = -1;
	}

//	dprintf("syscall_dispatcher: done with syscall 0x%x\n", call_num);

	return INT_RESCHEDULE;
}
