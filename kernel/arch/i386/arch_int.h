#ifndef _ARCH_INT_H
#define _ARCH_INT_H

/*
void set_intr_gate(int n, void *addr);
void set_trap_gate(int n, void *addr);
void set_system_gate(int n, void *addr);
*/
int arch_int_init(struct kernel_args *ka);
int arch_int_init2(struct kernel_args *ka);

void arch_int_enable_interrupts();
int arch_int_disable_interrupts();
void arch_int_restore_interrupts(int oldstate);
void arch_int_enable_io_interrupt(int irq);
void arch_int_disable_io_interrupt(int irq);

#endif

