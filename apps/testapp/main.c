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

	printf("test\n");

	printf("my thread id is %d\n", sys_get_current_thread_id());

	printf("enter something: ");

	for(;;) {
		c = getc();
		printf("%c", c);
	}

	for(;;) {
		sys_snooze(1000000);
		printf("booyah!");
	}

	for(;;);
	return 0;
}


