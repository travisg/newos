#ifndef _INTERRUPTS_H
#define _INTERRUPTS_H

void _page_fault_int();
void _general_protection_fault_int();
void _double_fault_int();
void _timer_int();

#if 0
void _default_int0();
void _default_int1();
void _default_int2();
void _default_int3();
void _default_int4();
void _default_int5();
void _default_int6();
void _default_int7();
void _default_int8();
void _default_int9();
void _default_int10();
void _default_int11();
void _default_int12();
void _default_int13();
void _default_int14();
void _default_int15();
#endif

#endif

