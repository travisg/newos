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
#include <kernel/vfs.h>
#include <kernel/arch/cpu.h>
#include <sys/errors.h>


#define LSHIFT  42
#define RSHIFT  54
#define SCRLOCK 70
#define NUMLOCK 69
#define CAPS    58
#define SYSREQ  55
#define F12     88

#define LED_SCROLL 1
#define LED_NUM    2
#define LED_CAPS   4

static bool shift;
static int  leds;
static sem_id keyboard_sem;
static mutex keyboard_read_mutex;
static char keyboard_buf[1024];
static unsigned int head, tail;
struct keyboard_fs {
	fs_id id;
	mutex lock;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

// stolen from nujeffos
const char unshifted_keymap[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, ' ',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, 0, 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, '\t' /*hack*/, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const char shifted_keymap[128] = {
	0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8, ' ',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, 0, 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void wait_for_output()
{
	while(in8(0x64) & 0x2)
		;
}

static void set_leds()
{
	wait_for_output();
	out8(0xed, 0x60);
	wait_for_output();
	out8(leds, 0x60);
}

static int _keyboard_read(void *_buf, size_t *len)
{
	unsigned int saved_tail;
	char *buf = _buf;
	size_t copied_bytes = 0;
	size_t copy_len;

	if(*len > sizeof(keyboard_buf) - 1)
		*len = sizeof(keyboard_buf) - 1;
	
retry:
	// block here until data is ready
	sem_acquire(keyboard_sem, 1);

	// critical section
	mutex_lock(&keyboard_read_mutex);
	
	saved_tail = tail;
	if(head == saved_tail) {
		mutex_unlock(&keyboard_read_mutex);
		goto retry;
	} else {
		// copy out of the buffer
		if(head < saved_tail)
			copy_len = min(*len, saved_tail - head);
		else
			copy_len = min(*len, sizeof(keyboard_buf) - head);
		memcpy(buf, &keyboard_buf[head], copy_len);	
		copied_bytes = copy_len;
		head = (head + copy_len) % sizeof(keyboard_buf);
		if(head == 0 && saved_tail > 0 && copied_bytes < *len) {
			// we wrapped around and have more bytes to read
			// copy the first part of the buffer
			copy_len = min(saved_tail, *len - copied_bytes);
			memcpy(&buf[*len], &keyboard_buf[0], copy_len);
			copied_bytes += copy_len;
			head = copy_len;
		}
	}
	if(head != saved_tail) {
		// we did not empty the keyboard queue
		sem_release_etc(keyboard_sem, 1, SEM_FLAG_NO_RESCHED);
	}
	
	mutex_unlock(&keyboard_read_mutex);
	
	*len = copied_bytes;	
	return 0;	
}

static void insert_in_buf(char c)
{
	unsigned int temp_tail = tail;

 	// see if the next char will collide with the head
	temp_tail = ++temp_tail % sizeof(keyboard_buf);
	if(temp_tail == head) {
		// buffer overflow, ditch this char
		return;
	}
	keyboard_buf[tail] = c;
	tail = temp_tail;
	sem_release_etc(keyboard_sem, 1, SEM_FLAG_NO_RESCHED);
}

int handle_keyboard_interrupt()
{
	unsigned char key;
	int retval = INT_NO_RESCHEDULE;
	
	key = in8(0x60);
//	dprintf("handle_keyboard_interrupt: key = 0x%x\n", key);

	if(key & 0x80) {
		// keyup
		if(key == LSHIFT + 0x80 || key == RSHIFT + 0x80)
			shift = false;
	} else {
		switch(key) {
			case LSHIFT:
			case RSHIFT:
				shift = true;
				break;
			case CAPS:
				if(leds & LED_CAPS)
					leds &= ~LED_CAPS;
				else
					leds |= LED_CAPS;
				set_leds();
				break;
			case SCRLOCK:
				if(leds & LED_SCROLL)
					leds &= ~LED_SCROLL;
				else
					leds |= LED_SCROLL;
				set_leds();
				break;
			case NUMLOCK:
				if(leds & LED_NUM)
					leds &= ~LED_NUM;
				else
					leds |= LED_NUM;
				set_leds();
				break;
			case SYSREQ:
				kernel_debugger();
				break;
			case F12:
				reboot();
				break;
			default: {
				char ascii;
				
				if(shift || (leds & LED_CAPS))
					ascii = shifted_keymap[key];
				else
					ascii = unshifted_keymap[key];
				
//					dprintf("ascii = 0x%x, '%c'\n", ascii, ascii);	
				if(ascii != 0) {
					insert_in_buf(ascii);
					retval = INT_RESCHEDULE;
				}
			}
		}
	}
	return retval;
}
static int keyboard_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct keyboard_fs *fs = _fs;
	int err;
	
//	dprintf("keyboard_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

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

static int keyboard_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
//	dprintf("keyboard_seek: entry\n");

	return ERR_NOT_ALLOWED;
}

static int keyboard_close(void *_fs, void *_vnode, void *_cookie)
{
//	dprintf("keyboard_close: entry\n");

	return 0;
}

static int keyboard_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct keyboard_fs *fs = _fs;
	
//	dprintf("keyboard_create: entry\n");

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

static int keyboard_stat(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir)
{
	struct keyboard_fs *fs = _fs;
	
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

static int keyboard_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	char c;
	int err;

//	dprintf("keyboard_read: entry\n");
	
	return _keyboard_read(buf, len);
}
static int keyboard_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	return ERR_VFS_READONLY_FS;
}

static int keyboard_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
}

static int keyboard_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct keyboard_fs *fs;
	int err;

	fs = kmalloc(sizeof(struct keyboard_fs));
	if(fs == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	err = mutex_init(&fs->lock, "keyboard_mutex");
	if(err < 0) {
		goto err1;
	}

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;

err1:	
	kfree(fs);
err:
	return err;
}

static int keyboard_unmount(void *_fs)
{
	struct keyboard_fs *fs = _fs;

	mutex_destroy(&fs->lock);
	kfree(fs);

	return 0;	
}

static int keyboard_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct keyboard_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int keyboard_unregister_mountpoint(void *_fs, void *_v)
{
	struct keyboard_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int keyboard_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

struct fs_calls keyboard_hooks = {
	&keyboard_mount,
	&keyboard_unmount,
	&keyboard_register_mountpoint,
	&keyboard_unregister_mountpoint,
	&keyboard_dispose_vnode,
	&keyboard_open,
	&keyboard_seek,
	&keyboard_read,
	&keyboard_write,
	&keyboard_ioctl,
	&keyboard_close,
	&keyboard_create,
	&keyboard_stat,
};

int setup_keyboard()
{
	keyboard_sem = sem_create(0, "keyboard_sem");
	if(keyboard_sem < 0)
		panic("could not create keyboard sem!\n");
	
	if(mutex_init(&keyboard_read_mutex, "keyboard_read_mutex") < 0)
		panic("could not create keyboard read mutex!\n");
	
	shift = 0;
	leds = 0;
	set_leds();
	
	head = tail = 0;
	
	return 0;
}

int	keyboard_dev_init(kernel_args *ka)
{
	setup_keyboard();
	int_set_io_interrupt_handler(0x21,&handle_keyboard_interrupt);
	
	vfs_register_filesystem("keyboard_dev_fs", &keyboard_hooks);
	sys_create("/dev", "", STREAM_TYPE_DIR);
	sys_create("/dev/keyboard", "", STREAM_TYPE_DIR);
	sys_mount("/dev/keyboard", "keyboard_dev_fs");
	return 0;
}	
