/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <newos/user_runtime.h>
#include <sys/syscalls.h>

extern int __stdio_init(void);
extern int __stdio_deinit(void);

extern void _kern_exit(int retcode);

extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);

extern void (*__dtor_list)(void);
extern void (*__dtor_end)(void);

extern int main(int argc,char **argv);

int _start(struct uspace_prog_args_t *);
void _call_ctors(void);
void _call_dtors(void);

char *__progname = "";
int errno = 0;

int _start(struct uspace_prog_args_t *uspa)
{
	int retcode;

	__stdio_init();
	_call_ctors();

	/* set up __progname */
	if(uspa->argv[0] != NULL) {
		char *temp;
		__progname = uspa->argv[0];
		for(temp = __progname; *temp != '\0'; temp++)
			if(*temp == '/')
				__progname = temp + 1;
	}

	retcode = main(uspa->argc, uspa->argv);

	_call_dtors();
	__stdio_deinit();

	_kern_exit(retcode);
	return 0;
}

void _call_ctors(void)
{ 
	void (**f)(void);

	for(f = &__ctor_list; f < &__ctor_end; f++) {
		(**f)();
	}
}

void _call_dtors(void)
{
	void (**f)(void);

	for (f = &__dtor_list; f < &__dtor_end; f++) {
		(**f)();
	}
}
