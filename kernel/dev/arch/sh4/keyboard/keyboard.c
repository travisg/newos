/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <libc/string.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <kernel/arch/cpu.h>
#include <sys/errors.h>

static mutex keyboard_read_mutex;

static int keyboard_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = NULL;
	return 0;
}

static int keyboard_close(dev_cookie cookie)
{
	return 0;
}

static int keyboard_freecookie(dev_cookie cookie)
{
	return 0;
}

static int keyboard_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t keyboard_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	if(len < 0)
		return 0;

	mutex_lock(&keyboard_read_mutex);

	return 0;
}

static ssize_t keyboard_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	return ERR_VFS_READONLY_FS;
}

static int keyboard_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

struct dev_calls keyboard_hooks = {
	&keyboard_open,
	&keyboard_close,
	&keyboard_freecookie,
	&keyboard_seek,
	&keyboard_ioctl,
	&keyboard_read,
	&keyboard_write,
	/* cannot page from keyboard */
	NULL,
	NULL,
	NULL
};

int setup_keyboard()
{
	if(mutex_init(&keyboard_read_mutex, "keyboard_read_mutex") < 0)
		panic("could not create keyboard read mutex!\n");

	return 0;
}

int	keyboard_dev_init(kernel_args *ka)
{
	setup_keyboard();

	devfs_publish_device("keyboard", NULL, &keyboard_hooks);

	return 0;
}
