#ifndef _ARCH_CONSOLE
#define _ARCH_CONSOLE

int arch_con_init(struct kernel_args *ka);
char arch_con_putch(char c);
void arch_con_puts(const char *s);
void arch_con_puts_xy(const char *s, int x, int y);

#endif

