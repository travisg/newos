/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
extern int __stdio_init();
extern int __stdio_deinit();
extern int __heap_init();
extern void sys_exit(int retcode);

extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);

extern int main();

void _call_ctors();

int _start()
{
	int retcode;

	_call_ctors();

	__heap_init();
	__stdio_init();

	retcode = main();

	__stdio_deinit();
	sys_exit(retcode);
	return 0;
}

void _call_ctors()
{ 
	void (**f)();
        
        for(f = &__ctor_list; f < &__ctor_end; f++) {
                (**f)();
        }
}

