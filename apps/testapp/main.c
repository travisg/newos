/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

int main()
{
	int fd;
	size_t len;
	char c;
	int rc = 0;

	printf("test\n");

	printf("my thread id is %d\n", sys_get_current_thread_id());
#if 0
	printf("enter something: ");

	for(;;) {
		c = getc();
		printf("%c", c);
	}
#endif
#if 0
	for(;;) {
		sys_snooze(100000);
		printf("booyah!");
	}

	for(;;);
#endif
#if 1
	printf("waiting 5 seconds\n");
	sys_snooze(5000000);
#endif
#if 1
	printf("doing some region tests\n");
	{
		region_id region;
		region_id region2;
		vm_region_info info;
		void *ptr, *ptr2;
		
		region = sys_vm_create_anonymous_region("foo", &ptr, REGION_ADDR_ANY_ADDRESS,
			16*4096, REGION_WIRING_LAZY, LOCK_RW);
		printf("region = 0x%x @ 0x%x\n", region, (unsigned int)ptr);
		region2 = sys_vm_clone_region("foo2", &ptr2, REGION_ADDR_ANY_ADDRESS,
			region, LOCK_RW);
		printf("region2 = 0x%x @ 0x%x\n", region2, (unsigned int)ptr2);
		
		sys_vm_get_region_info(region, &info);
		printf("info.base = 0x%x info.size = 0x%x\n", (unsigned int)info.base, (unsigned int)info.size);
		
		sys_vm_delete_region(region);
		sys_vm_delete_region(region2);
		printf("deleting both regions\n");
	}
#endif

// XXX dangerous! This overwrites the beginning of your hard drive
#if 0
	{
		char buf[512];		
		size_t bytes_read;

		printf("opening /dev/bus/ide/0/0/raw\n");
		fd = sys_open("/dev/bus/ide/0/0/raw", "", STREAM_TYPE_DEVICE);
		printf("fd = %d\n", fd);

		bytes_read = 512;
		rc = sys_read(fd, buf, 0, &bytes_read);
		printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

		buf[0] = 'f';
		buf[1] = 'o';
		buf[2] = 'o';
		buf[3] = '2';
		buf[4] = 0;

		bytes_read = 512;		
		rc = sys_write(fd, buf, 1024, &bytes_read);
		printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

		sys_close(fd);		
	}
#endif

	printf("exiting w/return code %d\n", rc);
	return rc;
}


