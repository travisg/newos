/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <nulibc/string.h>
#include <nulibc/stdio.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <kernel/arch/cpu.h>
#include <kernel/dev/arch/sh4/maple/maple_bus.h>
#include <newos/errors.h>

/* Stolen from KOS */
static char keymap_noshift[] = {
/*0*/	0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z',
/*1e*/	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
/*28*/	'\n', 27, 8, 9, 32, '-', '=', '[', ']', '\\', 0, ';', '\'',
/*35*/	'`', ',', '.', '/', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*46*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*53*/	0, '/', '*', '-', '+', 13, '1', '2', '3', '4', '5', '6',
/*5f*/	'7', '8', '9', '0', '.', 0
};
static char keymap_shift[] = {
/*0*/	0, 0, 0, 0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z',
/*1e*/	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
/*28*/	'\n', 27, 8, 9, 32, '_', '+', '{', '}', '|', 0, ':', '"',
/*35*/	'~', '<', '>', '?', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*46*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*53*/	0, '/', '*', '-', '+', 13, '1', '2', '3', '4', '5', '6',
/*5f*/	'7', '8', '9', '0', '.', 0
};
/* modifier keys */
#define KBD_MOD_LCTRL 		(1<<0)
#define KBD_MOD_LSHIFT		(1<<1)
#define KBD_MOD_LALT		(1<<2)
#define KBD_MOD_S1		(1<<3)
#define KBD_MOD_RCTRL		(1<<4)
#define KBD_MOD_RSHIFT		(1<<5)
#define KBD_MOD_RALT		(1<<6)
#define KBD_MOD_S2		(1<<7)

/* bits for leds : this is not comprensive (need for japanese kbds also) */
#define KBD_LED_NUMLOCK		(1<<0)
#define KBD_LED_CAPSLOCK	(1<<1)
#define KBD_LED_SCRLOCK		(1<<2)

/* defines for the keys (argh...) */
#define KBD_KEY_NONE		0x00
#define KBD_KEY_ERROR		0x01
#define KBD_KEY_A		0x04
#define KBD_KEY_B		0x05
#define KBD_KEY_C		0x06
#define KBD_KEY_D		0x07
#define KBD_KEY_E		0x08
#define KBD_KEY_F		0x09
#define KBD_KEY_G		0x0a
#define KBD_KEY_H		0x0b
#define KBD_KEY_I		0x0c
#define KBD_KEY_J		0x0d
#define KBD_KEY_K		0x0e
#define KBD_KEY_L		0x0f
#define KBD_KEY_M		0x10
#define KBD_KEY_N		0x11
#define KBD_KEY_O		0x12
#define KBD_KEY_P		0x13
#define KBD_KEY_Q		0x14
#define KBD_KEY_R		0x15
#define KBD_KEY_S		0x16
#define KBD_KEY_T		0x17
#define KBD_KEY_U		0x18
#define KBD_KEY_V		0x19
#define KBD_KEY_W		0x1a
#define KBD_KEY_X		0x1b
#define KBD_KEY_Y		0x1c
#define KBD_KEY_Z		0x1d
#define KBD_KEY_1		0x1e
#define KBD_KEY_2		0x1f
#define KBD_KEY_3		0x20
#define KBD_KEY_4		0x21
#define KBD_KEY_5		0x22
#define KBD_KEY_6		0x23
#define KBD_KEY_7		0x24
#define KBD_KEY_8		0x25
#define KBD_KEY_9		0x26
#define KBD_KEY_0		0x27
#define KBD_KEY_ENTER		0x28
#define KBD_KEY_ESCAPE		0x29
#define KBD_KEY_BACKSPACE	0x2a
#define KBD_KEY_TAB		0x2b
#define KBD_KEY_SPACE		0x2c
#define KBD_KEY_MINUS		0x2d
#define KBD_KEY_PLUS		0x2e
#define KBD_KEY_LBRACKET	0x2f
#define KBD_KEY_RBRACKET	0x30
#define KBD_KEY_BACKSLASH	0x31
#define KBD_KEY_SEMICOLON	0x33
#define KBD_KEY_QUOTE		0x34
#define KBD_KEY_TILDE		0x35
#define KBD_KEY_COMMA		0x36
#define KBD_KEY_PERIOD		0x37
#define KBD_KEY_SLASH		0x38
#define KBD_KEY_CAPSLOCK	0x39
#define KBD_KEY_F1		0x3a
#define KBD_KEY_F2		0x3b
#define KBD_KEY_F3		0x3c
#define KBD_KEY_F4		0x3d
#define KBD_KEY_F5		0x3e
#define KBD_KEY_F6		0x3f
#define KBD_KEY_F7		0x40
#define KBD_KEY_F8		0x41
#define KBD_KEY_F9		0x42
#define KBD_KEY_F10		0x43
#define KBD_KEY_F11		0x44
#define KBD_KEY_F12		0x45
#define KBD_KEY_PRINT		0x46
#define KBD_KEY_SCRLOCK		0x47
#define KBD_KEY_PAUSE		0x48
#define KBD_KEY_INSERT		0x49
#define KBD_KEY_HOME		0x4a
#define KBD_KEY_PGUP		0x4b
#define KBD_KEY_DEL		0x4c
#define KBD_KEY_END		0x4d
#define KBD_KEY_PGDOWN		0x4e
#define KBD_KEY_RIGHT		0x4f
#define KBD_KEY_LEFT		0x50
#define KBD_KEY_DOWN		0x51
#define KBD_KEY_UP		0x52
#define KBD_KEY_PAD_NUMLOCK	0x53
#define KBD_KEY_PAD_DIVIDE	0x54
#define KBD_KEY_PAD_MULTIPLY	0x55
#define KBD_KEY_PAD_MINUS	0x56
#define KBD_KEY_PAD_PLUS	0x57
#define KBD_KEY_PAD_ENTER	0x58
#define KBD_KEY_PAD_1		0x59
#define KBD_KEY_PAD_2		0x5a
#define KBD_KEY_PAD_3		0x5b
#define KBD_KEY_PAD_4		0x5c
#define KBD_KEY_PAD_5		0x5d
#define KBD_KEY_PAD_6		0x5e
#define KBD_KEY_PAD_7		0x5f
#define KBD_KEY_PAD_8		0x60
#define KBD_KEY_PAD_9		0x61
#define KBD_KEY_PAD_0		0x62
#define KBD_KEY_PAD_PERIOD	0x63
#define KBD_KEY_S3		0x65

typedef struct kbd_cond {
	uint8 modifiers;	/* bitmask of shift keys/etc */
	uint8 leds;		/* bitmask of leds that are lit */
	uint8 keys[6];		/* codes for up to 6 currently pressed keys */
} kbd_cond_t;

struct maple_keyboard {
	thread_id thread;
	int fd;
	bool caps;
	uint8 matrix[256];
	sem_id sem;
	mutex read_mutex;
	char buf[1024];
	unsigned int head, tail;
} keyboard;

static ssize_t _keyboard_read(struct maple_keyboard *k, void *_buf, size_t len);

static int keyboard_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = &keyboard;
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
	struct maple_keyboard *k = (struct maple_keyboard *)cookie;

	if(len < 0)
		return 0;

	return _keyboard_read(k, buf, len);
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

static ssize_t _keyboard_read(struct maple_keyboard *k, void *_buf, size_t len)
{
	unsigned int saved_tail;
	char *buf = _buf;
	size_t copied_bytes = 0;
	size_t copy_len;
	int rc;

	if(len > sizeof(k->buf) - 1)
		len = sizeof(k->buf) - 1;

retry:
	// block here until data is ready
	rc = sem_acquire_etc(k->sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
	if(rc == ERR_SEM_INTERRUPTED) {
		return 0;
	}

	// critical section
	mutex_lock(&k->read_mutex);

	saved_tail = k->tail;
	if(k->head == saved_tail) {
		mutex_unlock(&k->read_mutex);
		goto retry;
	} else {
		// copy out of the buffer
		if(k->head < saved_tail)
			copy_len = min(len, saved_tail - k->head);
		else
			copy_len = min(len, sizeof(k->buf) - k->head);
		memcpy(buf, &k->buf[k->head], copy_len);
		copied_bytes = copy_len;
		k->head = (k->head + copy_len) % sizeof(k->buf);
		if(k->head == 0 && saved_tail > 0 && copied_bytes < len) {
			// we wrapped around and have more bytes to read
			// copy the first part of the buffer
			copy_len = min(saved_tail, len - copied_bytes);
			memcpy(&buf[len], &k->buf[0], copy_len);
			copied_bytes += copy_len;
			k->head = copy_len;
		}
	}
	if(k->head != saved_tail) {
		// we did not empty the keyboard queue
		sem_release_etc(k->sem, 1, SEM_FLAG_NO_RESCHED);
	}

	mutex_unlock(&k->read_mutex);

	return copied_bytes;
}

static void enqueue_key(struct maple_keyboard *k, uint8 key, uint8 modifiers)
{
	char ascii = 0;
	bool shift;

//	dprintf("enqueue_key 0x%x 0x%x\n", key, modifiers);

	if(modifiers & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT))
		shift = ~k->caps;
	else
		shift = k->caps;

	switch(key) {
		case KBD_KEY_PRINT:
			panic("Keyboard Requested Halt\n");
			break;
		case KBD_KEY_CAPSLOCK:
			k->caps = !k->caps;
			return;
 		case KBD_KEY_F12:
			reboot();
			break;
		default:
			if(shift) {
				ascii = keymap_shift[key];
			} else {
				ascii = keymap_noshift[key];
			}
			break;
	}

	// add it to the keyboard queue
	if(ascii) {
		unsigned int temp_tail = k->tail;

//		dprintf("ascii = '%c' 0x%x\n", ascii, ascii);

	 	// see if the next char will collide with the head
		temp_tail++;
		temp_tail %= sizeof(k->buf);
		if(temp_tail == k->head) {
			// buffer overflow, ditch this char
			return;
		}
		k->buf[k->tail] = ascii;
		k->tail = temp_tail;
		sem_release_etc(k->sem, 1, SEM_FLAG_NO_RESCHED);
	}
}

static int keyboard_thread()
{
	struct maple_keyboard *k = &keyboard;
	maple_command cmd;
	maple_frame_t frame;
	uint32 outdata;
	uint32 indata[64];
	int err;
	kbd_cond_t *cond;
	int i;

	dprintf("keyboard_thread entry\n");

	cmd.cmd = MAPLE_COMMAND_GETCOND;
	cmd.outdatalen = 1;
	outdata = MAPLE_FUNC_KEYBOARD;
	cmd.outdata = &outdata;
	cmd.retframe = &frame;
	cmd.indata = indata;
	cmd.indatalen = (sizeof(kbd_cond_t) + sizeof(uint32)) / 4;

	for(;;) {
		thread_snooze(50);

		err = sys_ioctl(k->fd, MAPLE_IOCTL_SEND_COMMAND, &cmd, sizeof(cmd));
		if(err < NO_ERROR)
			continue;

		if(frame.cmd != MAPLE_RESPONSE_DATATRF
			|| (frame.datalen - 1) != sizeof(kbd_cond_t) / 4
			|| *((uint32 *)cmd.indata) != MAPLE_FUNC_KEYBOARD) {
			dprintf("error with frame\n");
			continue;
		}

		cond = (kbd_cond_t *)((uint32)cmd.indata + 4);

		for(i=0; i<6; i++) {
			if(cond->keys[i]) {
				if(k->matrix[cond->keys[i]] == 0)
					enqueue_key(k, cond->keys[i], cond->modifiers);
				k->matrix[cond->keys[i]] = 2;
			}
		}

		for(i=0; i<256; i++) {
			if(k->matrix[i] == 1)
				k->matrix[i] = 0;
			else if(k->matrix[i] == 2)
				k->matrix[i] = 1;
		}

//		dprintf("0x%x 0x%x 0x%x 0x%x ",
//			cond->modifiers, cond->leds, cond->keys[0], cond->keys[1]);
//		dprintf("0x%x 0x%x 0x%x 0x%x\n",
//			cond->keys[2], cond->keys[3], cond->keys[4], cond->keys[5], cond->keys[6]);
	}

	return 0;
}

int setup_keyboard()
{
	int i;

	memset(&keyboard, 0, sizeof(keyboard));

	if(mutex_init(&keyboard.read_mutex, "keyboard_read_mutex") < 0)
		panic("could not create keyboard read mutex!\n");

	keyboard.sem = sem_create(0, "keyboard_sem");
	if(keyboard.sem < 0)
		panic("could not create keyboard sem!\n");

	for(i=0; i<4; i++) {
		char temp[128];
		int err;
		uint32 func;

		sprintf(temp,  "/dev/bus/maple/%d/0/ctrl", i);
		keyboard.fd = sys_open(temp, STREAM_TYPE_DEVICE, 0);
		if(keyboard.fd < 0)
			continue;

		err = sys_ioctl(keyboard.fd, MAPLE_IOCTL_GET_FUNC, &func, sizeof(func));
		if(err < NO_ERROR || func != MAPLE_FUNC_KEYBOARD) {
			sys_close(keyboard.fd);
			keyboard.fd = -1;
			continue;
		}

		// we found one, break
		break;
	}
	if(keyboard.fd < 0)
		return -1;

	keyboard.thread = thread_create_kernel_thread("maple_keyboard_thread", &keyboard_thread, THREAD_HIGH_PRIORITY);
	thread_resume_thread(keyboard.thread);

	return 0;
}

int	keyboard_dev_init(kernel_args *ka)
{
	setup_keyboard();

	devfs_publish_device("keyboard", NULL, &keyboard_hooks);

	return 0;
}
