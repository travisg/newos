/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <newos/tty_priv.h>
#include <newos/key_event.h>

#include "consoled.h"

struct console {
	int console_fd;
	int keyboard_fd;
	int tty_master_fd;
	int tty_slave_fd;
	int tty_num;
	thread_id keyboard_reader;
	thread_id console_writer;
	sem_id wait_sem;
};

struct console theconsole;

static int console_reader(void *arg)
{
	char buf[1024];
	_key_event kevents[16];
	ssize_t len;
	ssize_t write_len;
	struct console *con = (struct console *)arg;

	for(;;) {
		len = read(con->keyboard_fd, kevents, sizeof(kevents));
		if(len < 0)
			break;

		len = process_key_events(kevents, len / sizeof(_key_event), buf, sizeof(buf), con->keyboard_fd);
		if(len <= 0)
			continue;

		write_len = write(con->tty_master_fd, buf, len);
	}

	_kern_sem_release(con->wait_sem, 1);

	return 0;
}

static int console_writer(void *arg)
{
	char buf[1024];
	ssize_t len;
	ssize_t write_len;
	struct console *con = (struct console *)arg;

	for(;;) {
		len = read(con->tty_master_fd, buf, sizeof(buf));
		if(len < 0)
			break;

		write_len = write(con->console_fd, buf, len);
	}

	_kern_sem_release(con->wait_sem, 1);

	return 0;
}

static int start_console(struct console *con)
{
	memset(con, 0, sizeof(struct console));

	con->wait_sem = _kern_sem_create(0, "console wait sem");
	if(con->wait_sem < 0)
		return -1;

	con->console_fd = open("/dev/console", 0);
	if(con->console_fd < 0)
		return -2;

	con->keyboard_fd = open("/dev/keyboard", 0);
	if(con->keyboard_fd < 0)
		return -3;

	con->tty_master_fd = open("/dev/tty/master", 0);
	if(con->tty_master_fd < 0)
		return -4;

	con->tty_num = ioctl(con->tty_master_fd, _TTY_IOCTL_GET_TTY_NUM, NULL, 0);
	if(con->tty_num < 0)
		return -5;

	{
		char temp[128];
		sprintf(temp, "/dev/tty/slave/%d", con->tty_num);

		con->tty_slave_fd = open(temp, 0);
		if(con->tty_slave_fd < 0)
			return -6;
	}

	con->keyboard_reader = _kern_thread_create_thread("console reader", &console_reader, con);
	if(con->keyboard_reader < 0)
		return -7;

	con->console_writer = _kern_thread_create_thread("console writer", &console_writer, con);
	if(con->console_writer < 0)
		return -8;

	_kern_thread_set_priority(con->keyboard_reader, 32);
	_kern_thread_set_priority(con->console_writer, 32);
	_kern_thread_resume_thread(con->keyboard_reader);
	_kern_thread_resume_thread(con->console_writer);

	return 0;
}

static proc_id start_process(const char *path, const char *name, char **argv, int argc, struct console *con)
{
	int saved_stdin, saved_stdout, saved_stderr;
	proc_id pid;

	saved_stdin = dup(0);
	saved_stdout = dup(1);
	saved_stderr = dup(2);

	dup2(con->tty_slave_fd, 0);
	dup2(con->tty_slave_fd, 1);
	dup2(con->tty_slave_fd, 2);

	// XXX launch
	pid = _kern_proc_create_proc(path, name, argv, argc, 5, PROC_FLAG_NEW_PGROUP);

	dup2(saved_stdin, 0);
	dup2(saved_stdout, 1);
	dup2(saved_stderr, 2);
	close(saved_stdin);
	close(saved_stdout);
	close(saved_stderr);

	return pid;
}

int main(void)
{
	int err;

	err = start_console(&theconsole);
	if(err < 0) {
		printf("consoled: error %d starting console\n", err);
		return err;
	}

	// we're a session leader
	setsid();

	// move our stdin and stdout to the console
	dup2(theconsole.tty_slave_fd, 0);
	dup2(theconsole.tty_slave_fd, 1);
	dup2(theconsole.tty_slave_fd, 2);

	for(;;) {
		proc_id shell_process;
		int retcode;
		char *argv[3];

#if 1
		argv[0] = "/boot/bin/shell";
		argv[1] = "-s";
		argv[2] = "/boot/loginscript";
		shell_process = start_process("/boot/bin/shell", "shell", argv, 3, &theconsole);
#else
		argv[0] = "/boot/bin/vmstat";
		shell_process = start_process("/boot/bin/vmstat", "vmstat", argv, 1, &theconsole);
#endif
		_kern_proc_wait_on_proc(shell_process, &retcode);
	}

	_kern_sem_acquire(theconsole.wait_sem, 1);

	return 0;
}

