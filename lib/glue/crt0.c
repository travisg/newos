/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
extern int __stdio_init();
extern int __stdio_deinit();
extern void sys_exit();

extern int main();

int _start()
{
	__stdio_init();
	
	main();
	
	__stdio_deinit();
	sys_exit();
	return 0;	
}
