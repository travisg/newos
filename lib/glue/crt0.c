/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
extern int __stdio_init();
extern int __stdio_deinit();
extern void sys_exit(int retcode);

extern int main();

int _start()
{
	int retcode;
	
	__stdio_init();
	
	retcode = main();
	
	__stdio_deinit();
	sys_exit(retcode);
	return 0;	
}
