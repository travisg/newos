/*
** Copyright 2003-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	vm_info_t info;
	int i;

	for(i = 0;; i++) {
		if((i % 20) == 0) {
			printf("   act inact  busy   mod  modt  free clear wired unused max commit page faults\n");
			printf("------------------------------------------------------------------------------\n");
		}

		int err = _kern_vm_get_vm_info(&info);
		if(err < 0) {
			printf("err %d in syscall\n", err);
			return -1;
		}

		printf("%6d%6d%6d%6d%6d%6d%6d%6d%7d%11d%12d\n",
			info.active_pages, info.inactive_pages, info.busy_pages, info.modified_pages, info.modified_temporary_pages,
			info.free_pages, info.clear_pages, info.wired_pages, info.unused_pages, info.max_commit, info.page_faults);

		sleep(1);
	}

	return 0;
}

