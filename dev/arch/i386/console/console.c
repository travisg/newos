/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/lock.h>
#include <kernel/vfs.h>
#include <sys/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <libc/string.h>
#include <libc/printf.h>

#include <dev/arch/i386/console/console_dev.h>


struct console_fs {
	int keyboard_fd;
	fs_id id;
	mutex lock;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

unsigned int origin = 0;

#define SCREEN_START 0xb8000
#define SCREEN_END   0xc0000
#define LINES 25
#define COLUMNS 80
#define NPAR 16

#define scr_end (origin+LINES*COLUMNS*2)

static unsigned int pos = 0;
static unsigned int x = 0;
static unsigned int y = 0;
static unsigned int bottom=LINES;
static unsigned int lines=LINES;
static unsigned int columns=COLUMNS;
static unsigned char attr=0x07;
static int vidport = 0;

static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
	if (new_x>=columns || new_y>=lines)
		return;
	x = new_x;
	y = new_y;
	pos = origin+((y*columns+x)<<1);
}

static void scrup(void)
{
	unsigned long i;
	
	// move the screen up one
	memcpy((void *)origin, (void *)origin+2*COLUMNS, 2*(LINES-1)*COLUMNS);
	
	// set the new position to the beginning of the last line
	pos = origin + (LINES-1)*COLUMNS*2;
	
	// clear the bottom line
	for(i = pos; i < scr_end; i += 2) {
		*(unsigned short *)i = 0x0720;
	}
}

static void lf(void)
{
	if (y+1<bottom) {
		y++;
		pos += columns<<1;
		return;
	}
	scrup();
}

static void cr(void)
{
	pos -= x<<1;
	x=0;
}

static void del(void)
{
	if (x > 0) {
		pos -= 2;
		x--;
		*(unsigned short *)pos = 0x0720;
	}
}

static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
	saved_x=x;
	saved_y=y;
}

static void restore_cur(void)
{
	x=saved_x;
	y=saved_y;
	pos=origin+((y*columns+x)<<1);
}

static char console_putch(const char c)
{
	if(++x>=COLUMNS) {
		cr();
		lf();
	}
	
	*(char *)pos = c;
	*(char *)(pos+1) = attr;

	pos += 2; 

	return c;
}

static int console_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct console_fs *fs = _fs;
	int err;
	
//	dprintf("console_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

	mutex_lock(&fs->lock);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		err = 0;
		goto out;		
	}		

	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE) {
		err = ERR_VFS_PATH_NOT_FOUND;
		goto err;
	}
	
	*_vnode = &fs->root_vnode;	
	*_cookie = NULL;

	err = 0;

out:
err:
	mutex_unlock(&fs->lock);

	return err;
}

static int console_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
//	dprintf("console_seek: entry\n");

	return ERR_NOT_ALLOWED;
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

	mutex_lock(&fs->lock);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		mutex_unlock(&fs->lock);
		return 0;
	}
	mutex_unlock(&fs->lock);
	
	return ERR_VFS_READONLY_FS;
}

static int console_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct console_fs *fs = _fs;
	
//	dprintf("console_stat: entry\n");

	mutex_lock(&fs->lock);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		mutex_unlock(&fs->lock);
		return 0;
	}
	mutex_unlock(&fs->lock);

	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE)
		return ERR_VFS_PATH_NOT_FOUND;

	stat->size = 0;
	return 0;
 }

static int console_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	char c;
	int err;
	struct console_fs *fs = _fs;
	
	return sys_read(fs->keyboard_fd,buf,0,len);
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

	mutex_lock(&fs->lock);

	err = _console_write(buf, len);

	mutex_unlock(&fs->lock);
	
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
			mutex_lock(&fs->lock);
			
			x = ((int *)buf)[0];
			y = ((int *)buf)[1];
			
			save_cur();
			gotoxy(x, y);
			_len = len - 2*sizeof(int);
			err = _console_write(((char *)buf) + 2*sizeof(int), &_len);
			restore_cur();
			mutex_unlock(&fs->lock);	
			break;
		}
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static int console_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct console_fs *fs;
	int err;

	fs = kmalloc(sizeof(struct console_fs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;
	fs->keyboard_fd = sys_open("/dev/keyboard","",STREAM_TYPE_DEVICE);
	if(fs->keyboard_fd < 0) {
		err = fs->keyboard_fd;
		goto err1;
	}
	err = mutex_init(&fs->lock, "console_mutex");
	if(err < 0) {
		err = ERR_NO_MEMORY;
		goto err2;
	}

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;

err2:
	sys_close(fs->keyboard_fd);
err1:
	kfree(fs);
err:
	return err;
}

static int console_unmount(void *_fs)
{
	struct console_fs *fs = _fs;
	sys_close(fs->keyboard_fd);
	mutex_destroy(&fs->lock);
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
	&console_stat,
};

int console_dev_init(kernel_args *ka)
{
	vidport = 0x3b4;

	dprintf("con_init: mapping vid mem\n");
	vm_map_physical_memory(vm_get_kernel_aspace_id(), "vid_mem", (void *)&origin, REGION_ADDR_ANY_ADDRESS,
		SCREEN_END - SCREEN_START, LOCK_RW|LOCK_KERNEL, SCREEN_START);
	dprintf("con_init: mapped vid mem to virtual address 0x%x\n", origin);

	pos = origin;

	gotoxy(0, ka->cons_line);

	// create device node
	vfs_register_filesystem("console_dev_fs", &console_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/console", "", STREAM_TYPE_DIR);
	sys_mount("/dev/console", "console_dev_fs");

	return 0;
}

