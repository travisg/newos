#ifndef _DEBUG_H
#define _DEBUG_H

#include <types.h>
#include "stage2.h"

// architecture independant
int dbg_init(struct kernel_args *ka);
char dbg_putch(char c);
void dbg_puts(const char *s);
bool dbg_set_serial_debug(bool new_val);
int dprintf(const char *fmt, ...);

#endif

