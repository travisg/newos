#ifndef _ARCH_DBG_CONSOLE
#define _ARCH_DBG_CONSOLE

#include <stage2.h>

char arch_dbg_con_read();
char arch_dbg_con_putch(char c);
void arch_dbg_con_puts(const char *s);
int arch_dbg_con_init(kernel_args *ka);

#endif

