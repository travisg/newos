/* 
** Copyright 2003, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/
#ifndef _KERNEL_SIGNAL_H
#define _KERNEL_SIGNAL_H

#include <kernel/timer.h>
#include <signal.h>

#define BLOCKABLE_SIGS	(~((1L << (SIGKILL - 1)) | (1L << (SIGSTOP - 1))))

extern int handle_signals(struct thread *t);

#define SIG_FLAG_NO_RESCHED 0x1

int send_signal_etc(thread_id tid, uint signal, uint32 flags);

int user_send_signal(thread_id tid, uint sig);
int user_send_proc_signal(thread_id tid, uint sig);
int user_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
bigtime_t user_set_alarm(bigtime_t time, timer_mode mode);

#endif	/* _KERNEL_SIGNAL_H */
