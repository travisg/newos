/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <newos/user_runtime.h>
#include <sys/syscalls.h>

extern int __stdio_init(void);
extern int __stdio_deinit(void);

extern void sys_exit(int retcode);

extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);

extern int main(int argc,char **argv);

int _start(struct uspace_prog_args_t *);
void _call_ctors(void);

char *__progname = "";
int errno = 0;

int _start(struct uspace_prog_args_t *uspa)
{
	int retcode;
	_call_ctors();

	__stdio_init();

	/* set up __progname */
	if(uspa->argv[0] != NULL) {
		char *temp;
		__progname = uspa->argv[0];
		for(temp = __progname; *temp != '\0'; temp++)
			if(*temp == '/')
				__progname = temp + 1;
	}

	retcode = main(uspa->argc, uspa->argv);

	__stdio_deinit();
	sys_exit(retcode);
	return 0;
}

void _call_ctors(void)
{ 
	void (**f)(void);

	for(f = &__ctor_list; f < &__ctor_end; f++) {
		(**f)();
	}
}

