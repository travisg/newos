#ifndef _ARCH_DBG_CONSOLE
#define _ARCH_DBG_CONSOLE

/*
char arch_dbg_con_putch(char c);
void arch_dbg_con_puts(const char *s);
int arch_dbg_con_init(struct kernel_args *ka);
*/

#define arch_dbg_con_putch(c) ((char)c)
#define arch_dbg_con_puts(s)
#define arch_dbg_con_init(ka) ((int)0)

#endif

