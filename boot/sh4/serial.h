#ifndef _SERIAL_H
#define _SERIAL_H

#include "defs.h"

int dprintf(const char *fmt, ...);
void serial_init();
void serial_putc(int c);
int serial_read();
void serial_puts(char *str);
void serial_write(u1 *data, int len);

#endif

