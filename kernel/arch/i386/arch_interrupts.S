/*
 *	interrupts.s
 */
.text

#define TRAP_ERRC(name, vector) \
.globl name; \
.align 8; \
name: \
	pushl	$vector; \
	jmp		int_bottom
	
#define TRAP(name, vector) \
.globl name; \
.align 8; \
name: \
	pushl	$0; \
	pushl	$vector; \
	jmp		int_bottom

TRAP(trap0, 0)
TRAP(trap1, 1)
TRAP(trap2, 2)
TRAP(trap3, 3)
TRAP(trap4, 4)
TRAP(trap5, 5)
TRAP(trap6, 6)
TRAP(trap7, 7)
TRAP_ERRC(trap8, 8)
TRAP(trap9, 9)
TRAP_ERRC(trap10, 10)
TRAP_ERRC(trap11, 11)
TRAP_ERRC(trap12, 12)
TRAP_ERRC(trap13, 13)
TRAP_ERRC(trap14, 14)
/*TRAP(trap15, 15)*/
TRAP(trap16, 16)
TRAP_ERRC(trap17, 17)
TRAP(trap18, 18)

TRAP(trap32, 32)
TRAP(trap33, 33)
TRAP(trap34, 34)
TRAP(trap35, 35)
TRAP(trap36, 36)
TRAP(trap37, 37)
TRAP(trap38, 38)
TRAP(trap39, 39)
TRAP(trap40, 40)
TRAP(trap41, 41)
TRAP(trap42, 42)
TRAP(trap43, 43)
TRAP(trap44, 44)
TRAP(trap45, 45)
TRAP(trap46, 46)
TRAP(trap47, 47)


.align 8
.globl int_bottom
int_bottom:
	pusha
	cld
	call	i386_handle_trap
	popa
	addl	$8,%esp
	iret
	
.globl _page_fault_int
.globl _general_protection_fault_int
.globl _ignore_int_low
.globl _ignore_int_hi
.globl _double_fault_int
.globl _timer_int

_page_fault_int:
	pusha
	cld
	movl 32(%esp),%eax
	movl 36(%esp),%ebx
	pushl %eax
	pushl %ebx
	call i386_page_fault	
	addl $8,%esp
	popa
	addl $4,%esp
	iret

_double_fault_int:
	pusha
	cld
	movl 32(%esp),%eax
	pushl %eax
	call i386_double_fault	
	addl $4,%esp
	popa
	addl $4,%esp
	iret

_general_protection_fault_int:
	pusha
	cld
	movl 32(%esp),%eax
	pushl %eax
	call i386_general_protection_fault	
	addl $4,%esp
	popa
	addl $4,%esp
	iret

.align 2
_timer_int:
	pusha
	cld
	push $0x20
	call interrupt_ack
	addl $4,%esp
	call i386_timer_interrupt
	popa
	iret

#if 0
/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown extern interrupt 0x%x\n"
#define DEFAULT_INT(name, vec) \
	.globl name; \
	.align 2; \
name: \
	pushl $vec; \
	jmp _ignore_int_bottom; \

	DEFAULT_INT(_default_int0, 0x20);
	DEFAULT_INT(_default_int1, 0x21);
	DEFAULT_INT(_default_int2, 0x22);
	DEFAULT_INT(_default_int3, 0x23);
	DEFAULT_INT(_default_int4, 0x24);
	DEFAULT_INT(_default_int5, 0x25);
	DEFAULT_INT(_default_int6, 0x26);
	DEFAULT_INT(_default_int7, 0x27);
	DEFAULT_INT(_default_int8, 0x28);
	DEFAULT_INT(_default_int9, 0x29);
	DEFAULT_INT(_default_int10, 0x2a);
	DEFAULT_INT(_default_int11, 0x2b);
	DEFAULT_INT(_default_int12, 0x2c);
	DEFAULT_INT(_default_int13, 0x2d);
	DEFAULT_INT(_default_int14, 0x2e);
	DEFAULT_INT(_default_int15, 0x2f);

.align 2
_ignore_int_bottom:
	pusha
	cld
	movl 32(%esp),%eax
	pushl %eax
	pushl $int_msg
	call dprintf
	call kprintf
	addl $4,%esp
	call interrupt_ack
	addl $4,%esp
	popa
	addl $4,%esp
	iret
#endif

