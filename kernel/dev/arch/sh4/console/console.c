/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/sem.h>
#include <kernel/debug.h>
#include <kernel/heap.h>

#include <libc/string.h>
#include <libc/printf.h>

#include <dev/arch/sh4/console/console_dev.h>
#include "keyboard.h"
#include "font.h"

struct console_fs {
	fs_id id;
	sem_id sem;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

static sem_id console_sem;
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

static int console_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct console_fs *fs = _fs;
	int err;
	
//	dprintf("console_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

	sem_acquire(fs->sem, 1);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		err = 0;
		goto out;		
	}		

	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE) {
		err = -1;
		goto err;
	}
	
	*_vnode = &fs->root_vnode;	
	*_cookie = NULL;

	err = 0;

out:
err:
	sem_release(fs->sem, 1);

	return err;
}

static int console_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
//	dprintf("console_seek: entry\n");

	return -1;
}

static int console_close(void *_fs, void *_vnode, void *_cookie)
{
//	dprintf("console_close: entry\n");

	return 0;
}

static int console_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct console_fs *fs = _fs;
	
//	dprintf("console_create: entry\n");

	sem_acquire(fs->sem, 1);
	
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		sem_release(fs->sem, 1);
		return 0;
	}
	sem_release(fs->sem, 1);
	
	return -1;
}

static int console_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	char c;
	int err;

//	dprintf("console_read: entry\n");
	
	return keyboard_read(buf, len);
}

static int _console_write(const void *buf, size_t *len)
{
	size_t i;
	const char *c;

	for(i=0; i<*len; i++) {
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

static int console_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	struct console_fs *fs = _fs;
	int err;

//	dprintf("console_write: entry, len = %d\n", *len);

	sem_acquire(fs->sem, 1);

	err = _console_write(buf, len);

	sem_release(fs->sem, 1);
	
	return err;
}

static int console_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	struct console_fs *fs = _fs;
	int err;
	
	switch(op) {
		case CONSOLE_OP_WRITEXY: {
			int x,y;
			size_t _len;
			sem_acquire(fs->sem, 1);
			
			x = ((int *)buf)[0];
			y = ((int *)buf)[1];
			
			save_cur();
			gotoxy(x, y);
			_len = len - 2*sizeof(int);
			err = _console_write(((char *)buf) + 2*sizeof(int), &_len);
			restore_cur();
			sem_release(fs->sem, 1);	
			break;
		}
		default:
			err = -1;
	}

	return err;
}

static int console_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct console_fs *fs;
	int err;

	fs = kmalloc(sizeof(struct console_fs));
	if(fs == NULL) {
		err = -1;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	{
		char temp[64];
		sprintf(temp, "console_sem%d", id);
		
		fs->sem = sem_create(1, temp);
		if(fs->sem < 0) {
			err = -1;
			goto err1;
		}
	}	

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;

err1:	
	kfree(fs);
err:
	return err;
}

static int console_unmount(void *_fs)
{
	struct console_fs *fs = _fs;

	sem_delete(fs->sem);
	kfree(fs);

	return 0;	
}

static int console_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct console_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int console_unregister_mountpoint(void *_fs, void *_v)
{
	struct console_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int console_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

struct fs_calls console_hooks = {
	&console_mount,
	&console_unmount,
	&console_register_mountpoint,
	&console_unregister_mountpoint,
	&console_dispose_vnode,
	&console_open,
	&console_seek,
	&console_read,
	&console_write,
	&console_ioctl,
	&console_close,
	&console_create,
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

	// create device node
	vfs_register_filesystem("console_dev_fs", &console_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/console", "", STREAM_TYPE_DIR);
	sys_mount("/dev/console", "console_dev_fs");

	return 0;
}

