#include <kernel.h>
#include <debug.h>
#include <int.h>
#include <sem.h>
#include <string.h>

#include <arch_cpu.h>

#include "console_dev.h"
#include "keyboard.h"

#define LSHIFT  42
#define RSHIFT  54
#define SCRLOCK 46
#define NUMLOCK 45
#define CAPS    58
#define SYSREQ  55
#define F12     88

#define LED_SCROLL 1
#define LED_NUM    2
#define LED_CAPS   4

static bool shift;
static int  leds;
static sem_id keyboard_sem;
static sem_id keyboard_read_sem;
static char keyboard_buf[1024];
static unsigned int head, tail;

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
	while(inb(0x64) & 0x2)
		;
}

static void set_leds()
{
	wait_for_output();
	outb(0xed, 0x60);
	wait_for_output();
	outb(leds, 0x60);
}

int keyboard_read(void *_buf, size_t *len)
{
	unsigned int saved_tail;
	char *buf = _buf;
	size_t copied_bytes = 0;
	size_t copy_len;
	
retry:
	// block here until data is ready
	sem_acquire(keyboard_sem, 1);

	// critical section
	sem_acquire(keyboard_read_sem, 1);

	saved_tail = tail;
	if(head == saved_tail) {
		sem_release(keyboard_read_sem, 1);
		goto retry;
	} else if(head < saved_tail) {
		// copy out of the buffer
		copy_len = min(*len, saved_tail - head);
		memcpy(buf, &keyboard_buf[head], copy_len);	
		copied_bytes = copy_len;
		head += copy_len;
	} else {
		// the buffer wraps around

		// copy the last part of the buffer
		copy_len = min(*len, sizeof(keyboard_buf) - head);
		memcpy(buf, &keyboard_buf[head], copy_len);
		copied_bytes = copy_len;
		head += copy_len;
		if(copied_bytes < *len) {
			// we still have buffer left, keep going		
			// copy the first part of the buffer
			copy_len = min(*len - copied_bytes, saved_tail);
			memcpy(&buf[*len], &keyboard_buf[0], copy_len);
			copied_bytes += copy_len;
			head = copy_len;
		}
	}

	sem_release(keyboard_read_sem, 1);

	*len = copied_bytes;

	return 0;	
}

static void insert_in_buf(char c)
{
	keyboard_buf[tail] = c;
	tail++;
	if(tail >= sizeof(keyboard_buf))
		tail = 0;
	sem_release_etc(keyboard_sem, 1, SEM_FLAG_NO_RESCHED);
}

int handle_keyboard_interrupt()
{
	unsigned char key;
	int retval = INT_NO_RESCHEDULE;
	
	key = inb(0x60);
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
				
//				dprintf("ascii = 0x%x, '%c'\n", ascii, ascii);	
				if(ascii != 0) {
					insert_in_buf(ascii);
					retval = INT_RESCHEDULE;
				}
			}		
		}
	}

	return retval;
}

int setup_keyboard()
{
	keyboard_sem = sem_create(0, "keyboard_sem");
	if(keyboard_sem < 0)
		panic("could not create keyboard sem!\n");
	
	keyboard_read_sem = sem_create(1, "keyboard_read_sem");
	if(keyboard_read_sem < 0)
		panic("could not create keyboard read sem!\n");
	
	shift = 0;
	leds = 0;
	set_leds();
	
	head = tail = 0;
	
	return 0;
}
