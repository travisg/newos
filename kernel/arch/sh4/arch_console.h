#ifndef _ARCH_CONSOLE
#define _ARCH_CONSOLE

/*
int arch_con_init(struct kernel_args *ka);
char arch_con_putch(char c);
void arch_con_puts(const char *s);
*/

#define arch_con_init(ka) ((int)0)
#define arch_con_putch(c) ((char)c)
#define arch_con_puts(s)
#define arch_con_puts_xy(s, x, y)

#endif

