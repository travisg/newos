#ifndef _ARCH_INT_H
#define _ARCH_INT_H

void set_intr_gate(int n, void *addr);
void set_trap_gate(int n, void *addr);
void set_system_gate(int n, void *addr);

int arch_int_init(struct kernel_args *ka);
int arch_int_init2(struct kernel_args *ka);

void arch_int_enable_interrupts();
int arch_int_disable_interrupts();
void arch_int_restore_interrupts(int oldstate);

//int arch_get_current_cpu();
#define arch_get_current_cpu() ((int)0)

#endif

