/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <types.h>

int handle_keyboard_interrupt();
int setup_keyboard();
ssize_t keyboard_read(void *buf, size_t len);

#endif
