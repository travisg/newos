#include <kernel/kernel.h>
#include <kernel/syscalls.h>
#include <kernel/int.h>
#include <kernel/debug.h>



int syscall_dispatcher(unsigned long call_num, unsigned long arg0, unsigned long arg1, unsigned long *call_ret)
{
	int ret;

	dprintf("syscall_dispatcher: call 0x%x, arg0 0x%x, arg1 0x%x\n",
		call_num, arg0, arg1);

	switch(call_num) {
		default:
			*call_ret = -1;
			ret = INT_NO_RESCHEDULE;
	}

	return ret;
}
