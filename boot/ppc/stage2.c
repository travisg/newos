/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
void _start()
{
	unsigned char *ptr;
	int i;
	
	ptr = (unsigned char *)0x96008000;

	for(i=0; i<1024*128;i++) {
		ptr[i] = i;
	}
	
	for(;;);
}
