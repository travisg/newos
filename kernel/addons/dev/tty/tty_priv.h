#ifndef _NEWOS_KERNEL_DEV_TTY_TTY_PRIV_H
#define _NEWOS_KERNEL_DEV_TTY_TTY_PRIV_H

#include <kernel/lock.h>

#define TTY_TRACE 0

#define NUM_TTYS 8
#define TTY_BUFFER_SIZE 4096

#define AVAILABLE_READ(buf)  (((buf)->line_start + (buf)->len - (buf)->tail) % (buf)->len)
#define AVAILABLE_WRITE(buf) (((buf)->tail + (buf)->len - (buf)->head - 1) % (buf)->len)
#define INC_HEAD(buf)        (buf)->head = ((buf)->head + 1) % (buf)->len
#define DEC_HEAD(buf)        { (buf)->head--; if((buf)->head < 0) (buf)->head = (buf)->len - 1; }
#define INC_TAIL(buf)        (buf)->tail = ((buf)->tail + 1) % (buf)->len
#define DEC_TAIL(buf)        { (buf)->tail--; if((buf)->tail < 0) (buf)->tail = (buf)->len - 1; }

#define ENDPOINT_MASTER_WRITE 0
#define ENDPOINT_SLAVE_READ 0
#define ENDPOINT_SLAVE_WRITE 1
#define ENDPOINT_MASTER_READ 1

enum {
	TTY_STATE_NORMAL = 0,
	TTY_STATE_WRITE_CR,
	TTY_STATE_WRITE_LF
};

struct line_buffer {
	int head;
	int tail;
	int line_start;
	int len;
	sem_id read_sem;
	sem_id write_sem;
	char buffer[TTY_BUFFER_SIZE];
	int state;
	int flags;
};

typedef struct tty_desc {
	bool inuse;
	int ref_count;
	int index;
	mutex lock;
	pgrp_id pgid;
	struct line_buffer buf[2]; /* one buffer in either direction */
} tty_desc;

typedef struct tty_global {
	mutex lock;

	tty_desc ttys[NUM_TTYS];
} tty_global;

extern tty_global thetty;
extern struct dev_calls ttys_hooks;
extern struct dev_calls ttym_hooks;

tty_desc *allocate_new_tty(void);
void deallocate_tty(tty_desc *tty);
void inc_tty_ref(tty_desc *tty);
void dec_tty_ref(tty_desc *tty);
ssize_t tty_read(tty_desc *tty, void *buf, ssize_t len, int endpoint);
ssize_t tty_write(tty_desc *tty, const void *buf, ssize_t len, int endpoint);
int tty_ioctl(tty_desc *tty, int op, void *buf, size_t len);

#endif

