#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <stage2.h>

// architecture independant
int con_init(kernel_args *ka);
char con_putch(char c);
void con_puts(const char *s);
void con_puts_xy(const char *s, int x, int y);
int kprintf(const char *fmt, ...);
int kprintf_xy(int x, int y, const char *fmt, ...);

#endif
