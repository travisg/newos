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
	unsigned long arg2, unsigned long arg3, unsigned long arg4, uint64 *call_ret)
{
	int ret = INT_NO_RESCHEDULE;

	dprintf("syscall_dispatcher: call 0x%x, arg0 0x%x, arg1 0x%x arg2 0x%x arg3 0x%x arg4 0x%x\n",
		call_num, arg0, arg1, arg2, arg3, arg4);

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
		case SYSCALL_SEM_ACQUIRE_ETC:
			*call_ret = sem_acquire_etc((sem_id)arg0, (int)arg1, (int)arg2, (time_t)INT32TOINT64(arg3, arg4));
			break;
		case SYSCALL_SEM_RELEASE:
			*call_ret = sem_release((sem_id)arg0, (int)arg1);
			break;
		case SYSCALL_SEM_RELEASE_ETC:
			*call_ret = sem_release_etc((sem_id)arg0, (int)arg1, (int)arg2);
			break;
		case SYSCALL_GET_CURRENT_THREAD_ID:
			*call_ret = thread_get_current_thread_id();
			break;
		default:
			*call_ret = -1;
			ret = INT_NO_RESCHEDULE;
	}

	dprintf("syscall_dispatcher: done with syscall 0x%x\n", call_num);

	return ret;
}
