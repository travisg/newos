/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <signal.h>
#include <errno.h>
#include <sys/syscalls.h>

static const char * const __sigstr[NSIG] = {
	"NONE", "HUP", "INT", "QUIT", "ILL", "CHLD", "ABRT", "PIPE",
	"FPE", "KILL", "STOP", "SEGV", "CONT", "TSTP", "ALRM", "TERM",
	"TTIN", "TTOU", "USR1", "USR2", "WINCH", "KILLTHR", "TRAP"
};

const char *strsignal(int sig)
{
	if(sig < 0 || sig >= NSIG)
		return NULL;

	return __sigstr[sig];
}

int
sigemptyset(sigset_t *set)
{
	*set = 0;
	return 0;
}

int
sigfillset(sigset_t *set)
{
	*set = ~0;
	return 0;
}

int 
sigaddset(sigset_t *set, int signo)
{
	sigset_t mask = (((sigset_t) 1) << (( signo ) - 1));
	return   ((*set |= mask), 0);
}

int 
sigdelset(sigset_t *set, int signo)
{
	sigset_t mask = (((sigset_t) 1) << (( signo ) - 1));	
	return   ((*set &= ~mask), 0);
}

int 
sigismember(const sigset_t *set, int signo)
{
	sigset_t mask = (((sigset_t) 1) << (( signo ) - 1));
	return   (*set & mask) ? 1 : 0 ;
}

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	return _kern_sigaction(sig, act, oact);
}

sig_func_t 
signal(int signal, sig_func_t signal_handler)
{
	struct sigaction sig, oldsig;
	int err;

	sig.sa_handler = signal_handler;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = 0;

	err = sigaction(signal, &sig, &oldsig);
	if(err < 0)
		return SIG_ERR;

	return oldsig.sa_handler;
}

int
send_signal(pid_t pid, uint sig)
{
	return _kern_send_proc_signal(pid, sig);
}

int
send_signal_thr(tid_t tid, uint sig)
{
	return _kern_send_signal(tid, sig);
}

int
kill(pid_t pid, int sig)
{
	return _kern_send_proc_signal(pid, sig);
}	

int
raise(int sig)
{
	return _kern_send_proc_signal(_kern_get_current_proc_id(), sig);
}
