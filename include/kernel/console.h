#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <boot/stage2.h>

int con_init(kernel_args *ka);
int kprintf(const char *fmt, ...);
int kprintf_xy(int x, int y, const char *fmt, ...);

#endif
