#ifndef _SERIAL_H
#define _SERIAL_H

int dprintf(const char *fmt, ...);
int serial_init();
char serial_putch(const char c);
void serial_puts(const char *str);

#endif

