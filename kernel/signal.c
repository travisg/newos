/* POSIX signals handling routines
** 
** Copyright 2002, Angelo Mottola, a.mottola@libero.it. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/
/* Adapted for NewOS by Travis Geiselbrecht
** Copyright 2004
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/signal.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/vm.h>

#include <string.h>
#include <signal.h>

#define NOISY_SIGNAL 0

#define SIGNAL_TO_MASK(signal) (1LL << (signal - 1))

const char * const sigstr[NSIG] = {
	"NONE", "HUP", "INT", "QUIT", "ILL", "CHLD", "ABRT", "PIPE",
	"FPE", "KILL", "STOP", "SEGV", "CONT", "TSTP", "ALRM", "TERM",
	"TTIN", "TTOU", "USR1", "USR2", "WINCH", "KILLTHR", "TRAP"
};


/** Expects interrupts off and thread lock held. */

int
handle_signals(struct thread *thread)
{
	uint32 signalMask = thread->sig_pending & (~thread->sig_block_mask);
	int i, sig, global_resched = 0;
	struct sigaction *handler;

#if NOISY_SIGNAL
	dprintf("handle_signals: thread 0x%x, sig_pending 0x%x, signalMask 0x%x\n", 
		thread->id, thread->sig_pending, signalMask);
#endif

	if (signalMask == 0)
		return 0;

	for (i = 0; i < NSIG; i++) {
		if (signalMask & 0x1) {
			sig = i + 1;
			handler = &thread->sig_action[i];
			signalMask >>= 1;
			thread->sig_pending &= ~(1L << i);

			dprintf("SIG: thread 0x%x received signal %s\n", thread->id, sigstr[sig]);

			if (handler->sa_handler == SIG_IGN) {
				// signal is to be ignored
				// XXX apply zombie cleaning on SIGCHLD
				continue;
			}
			if (handler->sa_handler == SIG_DFL) {
				// default signal behaviour
				switch (sig) {
					case SIGCHLD:
					case SIGWINCH:
					case SIGCONT:
						continue;

					case SIGSTOP:
					case SIGTSTP:
					case SIGTTIN:
					case SIGTTOU:
						thread->next_state = THREAD_STATE_SUSPENDED;
						global_resched = 1;
						continue;

					case SIGHUP:
					case SIGINT:
					case SIGQUIT:
					case SIGILL:
					case SIGABRT:
					case SIGPIPE:
					case SIGFPE:
					case SIGSEGV:
					case SIGALRM:
					case SIGTRAP:
						dprintf("Shutting down thread 0x%x due to signal #%d (%s)\n", thread->id, sig, sigstr[sig]);
					case SIGKILL:
					case SIGKILLTHR:
					default:
						RELEASE_THREAD_LOCK();
						int_restore_interrupts();
						thread_exit(ERR_TASK_THREAD_KILLED);
				}
			}

			// XXX it's not safe to call arch_setup_signal_frame with interrupts disabled
			// since it writes to the user stack and may page fault.
			//
			// User defined signal handler
			dprintf("### Setting up custom signal handler frame...\n");
			arch_setup_signal_frame(thread, handler, sig, thread->sig_block_mask);

			if (handler->sa_flags & SA_ONESHOT)
				handler->sa_handler = SIG_DFL;
			if (!(handler->sa_flags & SA_NOMASK))
				thread->sig_block_mask |= (handler->sa_mask | (1L << (sig-1))) & BLOCKABLE_SIGS;

			return global_resched;
		} else
			signalMask >>= 1;
	}
	arch_check_syscall_restart(thread);

	return global_resched;
}


int
send_signal_etc(thread_id threadID, uint signal, uint32 flags)
{
	struct thread *thread;

	if (signal < 1 || signal > MAX_SIGNO)
		return ERR_INVALID_ARGS;

	int_disable_interrupts();
	GRAB_THREAD_LOCK();

	thread = thread_get_thread_struct_locked(threadID);
	if (!thread) {
		RELEASE_THREAD_LOCK();
		int_restore_interrupts();
		return ERR_INVALID_HANDLE;
	}
	// XXX check permission

	dprintf("SIG: sending signal %s to thread 0x%x (pending 0x%x, \n", sigstr[signal], thread->id);

	if (thread->proc == proc_get_kernel_proc()) {
		// Signals to kernel threads will only wake them up
		if (thread->state == THREAD_STATE_SUSPENDED) {
			thread->state = thread->next_state = THREAD_STATE_READY;
			thread_enqueue_run_q(thread);
		}
	} else {
		thread->sig_pending |= SIGNAL_TO_MASK(signal);

		switch (signal) {
			case SIGKILL:
			{
				struct thread *mainThread = thread->proc->main_thread;
				// Forward KILLTHR to the main thread of the team

				mainThread->sig_pending |= SIGNAL_TO_MASK(SIGKILL);
				// Wake up main thread
				if (mainThread->state == THREAD_STATE_SUSPENDED) {
					mainThread->state = mainThread->next_state = THREAD_STATE_READY;
					thread_enqueue_run_q(mainThread);
				} else if (mainThread->state == THREAD_STATE_WAITING)
					sem_interrupt_thread(mainThread);

				// Supposed to fall through
			}
			case SIGKILLTHR:
				// Wake up suspended threads and interrupt waiting ones
				if (thread->state == THREAD_STATE_SUSPENDED) {
					thread->state = thread->next_state = THREAD_STATE_READY;
					thread_enqueue_run_q(thread);
				} else if (thread->state == THREAD_STATE_WAITING)
					sem_interrupt_thread(thread);
				break;
			case SIGCONT:
				// Wake up thread if it was suspended
				if (thread->state == THREAD_STATE_SUSPENDED) {
					thread->state = thread->next_state = THREAD_STATE_READY;
					thread_enqueue_run_q(thread);
				}
				break;
			default:
				if (thread->sig_pending & (~thread->sig_block_mask | SIGNAL_TO_MASK(SIGCHLD))) {
					// Interrupt thread if it was waiting
					if (thread->state == THREAD_STATE_WAITING)
						sem_interrupt_thread(thread);
				}
				break;
		}
	}
	
	if (!(flags & SIG_FLAG_NO_RESCHED))
		thread_resched();

	RELEASE_THREAD_LOCK();
	int_restore_interrupts();

	return 0;
}


static int
send_signal(thread_id tid, uint signal)
{
	return send_signal_etc(tid, signal, 0);
}

int
send_proc_signal_etc(proc_id pid, uint signal, uint32 flags)
{
	thread_id tid = proc_get_main_thread(pid);
	return send_signal_etc(tid, signal, flags);
}

static int
send_proc_signal(proc_id pid, uint signal)
{
	return send_proc_signal_etc(pid, signal, 0);
}

static int
has_signals_pending(void *_thread)
{
	struct thread *thread = (struct thread *)_thread;
	if (thread == NULL)
		thread = thread_get_current_thread();

	return thread->sig_pending & ~thread->sig_block_mask;
}


static int
sigaction(int signal, const struct sigaction *act, struct sigaction *oact)
{
	struct thread *thread;

	if (signal < 1 || signal > MAX_SIGNO
		|| signal == SIGKILL || signal == SIGKILLTHR || signal == SIGSTOP)
		return ERR_INVALID_ARGS;

	int_disable_interrupts();
	GRAB_THREAD_LOCK();

	thread = thread_get_current_thread();

	if (oact)
		memcpy(oact, &thread->sig_action[signal - 1], sizeof(struct sigaction));
	if (act)
		memcpy(&thread->sig_action[signal - 1], act, sizeof(struct sigaction));

	if (act && act->sa_handler == SIG_IGN)
		thread->sig_pending &= ~SIGNAL_TO_MASK(signal);
	else if (act && act->sa_handler == SIG_DFL) {
		if (signal == SIGCONT || signal == SIGCHLD || signal == SIGWINCH)
			thread->sig_pending &= ~SIGNAL_TO_MASK(signal);
	} /*else
		dprintf("### custom signal handler set\n");*/

	RELEASE_THREAD_LOCK();
	int_restore_interrupts();

	return 0;
}


// Triggers a SIGALRM to the thread that issued the timer and reschedules

static int
alarm_event(void *arg)
{
	thread_id tid = (thread_id)arg;

	send_signal_etc(tid, SIGALRM, SIG_FLAG_NO_RESCHED);

	return INT_RESCHEDULE;
}


static bigtime_t
set_alarm(bigtime_t time, timer_mode mode)
{
	struct thread *t = thread_get_current_thread();
	bigtime_t rv = 0;

	int_disable_interrupts();
	GRAB_THREAD_LOCK();

	timer_cancel_event(&t->alarm_event);

	if (time >= 0) {
		timer_setup_timer(&alarm_event, (void *)t->id, &t->alarm_event);
		timer_set_event(time, mode, &t->alarm_event);
	}

	RELEASE_THREAD_LOCK();
	int_restore_interrupts();

	return rv;
}

bigtime_t
user_set_alarm(bigtime_t time, timer_mode mode)
{
	return set_alarm(time, mode);
}

int
user_send_signal(thread_id tid, uint signal)
{
	return send_signal(tid, signal);
}

int
user_send_proc_signal(proc_id pid, uint signal)
{
	return send_proc_signal(pid, signal);
}

int
user_sigaction(int sig, const struct sigaction *userAction, struct sigaction *userOldAction)
{
	struct sigaction act, oact;
	int rc;

	if(userAction == NULL)
		return ERR_INVALID_ARGS;

	if(is_kernel_address(userAction))
		return ERR_VM_BAD_USER_MEMORY;

	if (user_memcpy(&act, userAction, sizeof(struct sigaction)) < 0)
		return ERR_VM_BAD_USER_MEMORY;

	if(userOldAction) {
		if(is_kernel_address(userOldAction))
			return ERR_VM_BAD_USER_MEMORY;
		if(user_memcpy(&oact, userOldAction, sizeof(struct sigaction)) < 0)
			return ERR_VM_BAD_USER_MEMORY;
	}

	rc = sigaction(sig, userAction ? &act : NULL, userOldAction ? &oact : NULL);

	// only copy the old action if a pointer has been given
	if(rc >= 0 && userOldAction) {
		if(user_memcpy(userOldAction, &oact, sizeof(struct sigaction)) < 0)
			return ERR_VM_BAD_USER_MEMORY;
	}
	
	return rc;
}

