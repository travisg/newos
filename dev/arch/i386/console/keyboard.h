#ifndef _KEYBOARD_H
#define _KEYBOARD_H

int handle_keyboard_interrupt();
int setup_keyboard();
int keyboard_read(void *buf, size_t *len);

#endif
