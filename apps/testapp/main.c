#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libsys/syscalls.h>

int a;
int b = 1;

int main();

int _start()
{
	return main();
}

int main()
{
	int fd;
	size_t len;
	
	fd = sys_open("/dev/console", "", STREAM_TYPE_DEVICE);
	if(fd < 0)
		return -1;

	for(;;) {
		sys_snooze(1000000);
		len = strlen("booyah!");
		sys_write(fd, "booyah!", 0, &len);
	}

	for(;;);
	return 0;
}


