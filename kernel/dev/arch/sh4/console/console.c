/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/sem.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/fs/devfs.h>
#include <sys/errors.h>

#include <libc/string.h>
#include <libc/printf.h>

#include <kernel/dev/arch/sh4/console/console_dev.h>
#include "keyboard.h"
#include "font.h"

static mutex console_lock;
static uint16 *vid_mem;
static int num_cols;
static int num_rows;
static int x_pixels, y_pixels;
static int x, y;
static int saved_x, saved_y;
static uint16 background_color;

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 12

static void gotoxy(int new_x, int new_y)
{
	x = new_x;
	y = new_y;
}

static void scrup()
{
	// move the screen up one
	memcpy(vid_mem, vid_mem + x_pixels * CHAR_HEIGHT, (x_pixels * y_pixels - x_pixels * CHAR_HEIGHT) * 2);

	// clear the bottom line
	memset(vid_mem + x_pixels * CHAR_HEIGHT * (num_rows - 1), 0, x_pixels * 2);
}

static void cr()
{
	x = 0;
}

static void lf()
{
	if(y+1 >= num_rows) {
		scrup();
	} else {
		y++;
	}
}

static void del()
{
}

static void save_cur()
{
	saved_x = x;
	saved_y = y;
}

static void restore_cur()
{
	x = saved_x;
	y = saved_y;
}

static void draw_char(int x, int y, char c, uint16 color)
{
	int line;
	int col;
	uint8 *font_char = FONT + CHAR_HEIGHT * (c & 0x7f);

	for(line = 0; line < CHAR_HEIGHT; line++) {
		uint16 *vid_spot = &vid_mem[(y+line)*x_pixels + x];
		uint8 bits = *font_char++;

		for(col = 0; col < CHAR_WIDTH; col++) {
			if(bits & 1) *vid_spot = color;
			else *vid_spot = background_color;
			bits >>= 1;
			vid_spot++;
		}
	}
}

static char console_putch(const char c)
{
	if(++x >= num_cols) {
		cr();
		lf();
	}

	draw_char(x * CHAR_WIDTH, y * CHAR_HEIGHT, c, 0xffff);

	return c;
}

static int console_open(dev_ident ident, dev_cookie *cookie)
{
	return 0;
}

static int console_freecookie(dev_cookie cookie)
{
	return 0;
}

static int console_seek(dev_cookie cookie, off_t pos, seek_type st)
{
//	dprintf("console_seek: entry\n");

	return ERR_NOT_ALLOWED;
}

static int console_close(dev_cookie cookie)
{
//	dprintf("console_close: entry\n");

	return 0;
}

static ssize_t console_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
//	dprintf("console_read: entry\n");

	return keyboard_read(buf, len);
}

static ssize_t _console_write(const void *buf, size_t len)
{
	size_t i;
	const char *c;

	for(i=0; i<len; i++) {
		c = &((const char *)buf)[i];
		switch(*c) {
			case '\n':
				cr();
				lf();
				break;
			case '\r':
				cr();
				break;
			case 0x8: // backspace
				del();
				break;
			case '\0':
				break;
			default:
				console_putch(*c);
		}
	}
	return 0;
}

static ssize_t console_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	ssize_t err;

//	dprintf("console_write: entry, len = %d\n", *len);

	mutex_lock(&console_lock);

	err = _console_write(buf, len);

	mutex_unlock(&console_lock);

	return err;
}

static int console_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	int err;

	switch(op) {
		case CONSOLE_OP_WRITEXY: {
			int x,y;
			size_t _len;
			mutex_lock(&console_lock);

			x = ((int *)buf)[0];
			y = ((int *)buf)[1];

			save_cur();
			gotoxy(x, y);
			_len = len - 2*sizeof(int);
			err = _console_write(((char *)buf) + 2*sizeof(int), _len);
			restore_cur();
			mutex_unlock(&console_lock);
			break;
		}
		default:
			err = -1;
	}

	return err;
}

struct dev_calls console_hooks = {
	&console_open,
	&console_close,
	&console_freecookie,
	&console_seek,
	&console_ioctl,
	&console_read,
	&console_write,
	/* cannot page from /dev/console */
	NULL,
	NULL,
	NULL
};

int console_dev_init(kernel_args *ka)
{
	int i;

	dprintf("console_dev_init: entry\n");

	// set up the video memory pointer
	vid_mem = (uint16 *)0xa5000000;

	// clear the screen
	for(i = 0; i<8*1024*1024/2; i++) {
		vid_mem[i] = 0;
	}

	// set up some stuff
	x_pixels = 640;
	y_pixels = 480;
	num_cols = x_pixels / CHAR_WIDTH;
	num_rows = y_pixels / CHAR_HEIGHT;
	x = 0;
	y = 0;
	background_color = 0;

	mutex_init(&console_lock, "console_lock");

	// create device node
	devfs_publish_device("console", NULL, &console_hooks);

	return 0;
}

