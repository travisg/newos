/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define FUNC(name) .align 2 ; .globl _##name ; _##name:

FUNC(sys_null)
	trapa	#0
	rts
	nop

FUNC(sys_open)
	trapa	#1
	rts
	nop

FUNC(sys_seek)
	trapa	#2
	rts
	nop

FUNC(sys_read)
	mov.l	@r15,r0
	trapa	#3
	rts
	nop

FUNC(sys_write)
	mov.l	@r15,r0
	trapa	#4
	rts
	nop

FUNC(sys_ioctl)
	trapa	#5
	rts
	nop

FUNC(sys_close)
	trapa	#6
	rts
	nop

FUNC(sys_create)
	trapa	#7
	rts
	nop

FUNC(sys_stat)
	trapa	#8
	rts
	nop

FUNC(sys_system_time)
	trapa	#9
	rts
	nop

FUNC(sys_snooze)
	trapa	#10
	rts
	nop

FUNC(sys_sem_create)
	trapa	#11
	rts
	nop

FUNC(sys_sem_delete)
	trapa	#12
	rts
	nop

FUNC(sys_sem_acquire)
	trapa	#13
	rts
	nop


FUNC(sys_sem_acquire_etc)
	mov.l	@r15,r0
	trapa	#14
	rts
	nop

FUNC(sys_sem_release)
	trapa	#15
	rts
	nop

FUNC(sys_sem_release_etc)
	trapa	#16
	rts
	nop

FUNC(sys_get_current_thread_id)
	trapa	#17
	rts
	nop


