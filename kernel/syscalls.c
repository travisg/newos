#include <kernel/kernel.h>
#include <kernel/syscalls.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/arch/cpu.h>

int syscall_dispatcher(unsigned long call_num, unsigned long arg0, unsigned long arg1,
	unsigned long arg2, unsigned long arg3, unsigned long arg4, uint64 *call_ret)
{
	int ret = INT_RESCHEDULE;

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
			*call_ret = user_seek((int)arg0, ((off_t)arg1 | ((off_t)arg2 << 32)), (seek_type)arg3);
			break;
		case SYSCALL_READ:
			*call_ret = user_read((int)arg0, (void *)arg1, ((off_t)arg2 | ((off_t)arg3 << 32)), (size_t *)arg4);
			break;
		case SYSCALL_WRITE:
			*call_ret = user_write((int)arg0, (const void *)arg1, ((off_t)arg2 | ((off_t)arg3 << 32)), (size_t *)arg4);
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
			thread_snooze((time_t)arg0 | ((time_t)arg1 << 32));
			*call_ret = 0;
			break;
		default:
			*call_ret = -1;
			ret = INT_NO_RESCHEDULE;
	}

	return ret;
}
