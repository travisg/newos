#define	FUNC(name) .align 2 ; .globl _##name ; _##name:

.text
FUNC(reboot)
	rts
	nop

FUNC(system_time)
	rts
	nop

FUNC(atomic_add)
	rts
	nop

FUNC(atomic_and)
	rts
	nop

FUNC(atomic_or)
	rts
	nop

FUNC(test_and_set)
	rts
	nop

