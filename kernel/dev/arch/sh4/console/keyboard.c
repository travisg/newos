/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>

#include "keyboard.h"

int keyboard_read(void *_buf, size_t *len)
{
	return -1;	
}

int handle_keyboard_interrupt()
{
	return 0;
}

int setup_keyboard()
{
	return 0;
}
