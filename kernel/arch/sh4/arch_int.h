#ifndef _ARCH_INT_H
#define _ARCH_INT_H

/*
int arch_int_init(struct kernel_args *ka);
int arch_int_init2(struct kernel_args *ka);
void arch_int_enable_interrupts();
void arch_int_disable_interrupts();
*/

#define arch_int_init(ka) ((int)0)
#define arch_int_init2(ka) ((int)0)
#define arch_int_enable_interrupts()
#define arch_int_disable_interrupts() ((int)0)
#define arch_int_restore_interrupts(x)
#define arch_get_current_cpu() ((int)0)

#endif

