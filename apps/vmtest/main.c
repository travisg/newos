/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscalls.h>
#include <newos/errors.h>

int main(void)
{
	int rc = 0;

	printf("VM test\n");

	printf("my thread id is %d\n", _kern_get_current_thread_id());
#if 0
	printf("writing to invalid page\n");
	*(int *)0x30000000 = 5;
#endif
#if 1
	printf("doing some region tests\n");
	{
		region_id region;
		region_id region2;
		vm_region_info info;
		void *ptr, *ptr2;

		region = _kern_vm_create_anonymous_region("foo", &ptr, REGION_ADDR_ANY_ADDRESS,
			16*4096, REGION_WIRING_LAZY, LOCK_RW);
		printf("region = 0x%x @ 0x%x\n", region, (unsigned int)ptr);
		region2 = _kern_vm_clone_region("foo2", &ptr2, REGION_ADDR_ANY_ADDRESS,
			region, REGION_NO_PRIVATE_MAP, LOCK_RW);
		printf("region2 = 0x%x @ 0x%x\n", region2, (unsigned int)ptr2);

		_kern_vm_get_region_info(region, &info);
		printf("info.base = 0x%x info.size = 0x%x\n", (unsigned int)info.base, (unsigned int)info.size);

		_kern_vm_delete_region(region);
		_kern_vm_delete_region(region2);
		printf("deleting both regions\n");
	}
#endif
#if 1
	printf("doing some commitment tests (will only be proper on 512Mb machines)\n");
	{
		region_id region;
		region_id region2;
		void *ptr, *ptr2;

		region = _kern_vm_create_anonymous_region("large", &ptr, REGION_ADDR_ANY_ADDRESS,
			450*1024*1024, REGION_WIRING_LAZY, LOCK_RW);
		if(region < 0) {
			printf("error %d creating large region\n", region);
		} else {
			printf("region = 0x%x @ 0x%x\n", region, (unsigned int)ptr);
		}

		printf("pausing for 5 seconds...\n");
		_kern_snooze(5000000);
		region2 = _kern_vm_create_anonymous_region("large2", &ptr2, REGION_ADDR_ANY_ADDRESS,
			64*1024*1024, REGION_WIRING_LAZY, LOCK_RW);
		if(region2 < 0) {
			printf("error %d creating large region\n", region2);
			if(region2 == ERR_VM_WOULD_OVERCOMMIT)
				printf("good, it failed because of overcommitting\n");
		} else {
			printf("region2 = 0x%x @ 0x%x\n", region2, (unsigned int)ptr2);
		}

		printf("pausing for 5 seconds...\n");
		_kern_snooze(5000000);
		printf("deleting both regions\n");
		_kern_vm_delete_region(region);
		_kern_vm_delete_region(region2);
	}
#endif
#if 1
	{
		// create a couple of big regions and time it
		region_id region;
		void *ptr;
		bigtime_t t;

		printf("running some timing tests on region creation/deletion.\n");
		printf("you'll want to make sure serial debugging is off or the test will take forever,\n");
		printf(" use the scroll lock key to toggle it on or off.\n");
		printf("pausing for 2 seconds...\n");
		_kern_snooze(2000000);

		t = _kern_system_time();
		region = _kern_vm_create_anonymous_region("large", &ptr, REGION_ADDR_ANY_ADDRESS,
			64*1024*1024, REGION_WIRING_LAZY, LOCK_RW);
		t = _kern_system_time() - t;

		printf("took %d microseconds to create large region (lazy wiring)\n", (int)t);

		t = _kern_system_time();
		_kern_vm_delete_region(region);
		t = _kern_system_time() - t;

		printf("took %d microseconds to delete it (with no pages allocated)\n", (int)t);

		t = _kern_system_time();
		region = _kern_vm_create_anonymous_region("large", &ptr, REGION_ADDR_ANY_ADDRESS,
			64*1024*1024, REGION_WIRING_WIRED, LOCK_RW);
		t = _kern_system_time() - t;

		printf("took %d microseconds to create large region (wired)\n", (int)t);

		t = _kern_system_time();
		_kern_vm_delete_region(region);
		t = _kern_system_time() - t;

		printf("took %d microseconds to delete it (with all allocated)\n", (int)t);
	}
#endif

	printf("vmtest: exiting w/return code %d\n", rc);
	return rc;
}


