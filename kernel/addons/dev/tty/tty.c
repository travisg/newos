/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/* Modified by Justin Smith 2003-07-08 */
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/sem.h>
#include <kernel/lock.h>
#include <kernel/dev/fixed.h>
#include <kernel/fs/devfs.h>
#include <newos/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <string.h>
#include <stdio.h>

#include "tty_priv.h"

#include <newos/tty_priv.h>

#if TTY_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

tty_global thetty;

tty_desc *allocate_new_tty(void)
{
	int i;
	tty_desc *tty = NULL;

	mutex_lock(&thetty.lock);

	for(i=0; i<NUM_TTYS; i++) {
		if(thetty.ttys[i].inuse == false) {
			ASSERT(thetty.ttys[i].ref_count == 0);
			thetty.ttys[i].inuse = true;
			thetty.ttys[i].ref_count = 1;
			tty = &thetty.ttys[i];
			break;
		}
	}

	mutex_unlock(&thetty.lock);

	return tty;
}

void inc_tty_ref(tty_desc *tty)
{
	mutex_lock(&thetty.lock);
	tty->ref_count++;
	if(tty->ref_count == 1)
		tty->inuse = true;
	mutex_unlock(&thetty.lock);
}

void dec_tty_ref(tty_desc *tty)
{
	mutex_lock(&thetty.lock);
	tty->ref_count--;
	if(tty->ref_count == 0)
		tty->inuse = false;
	mutex_unlock(&thetty.lock);
}

static int tty_insert_char(struct line_buffer *lbuf, char c, bool move_line_start)
{
	bool was_empty = (AVAILABLE_READ(lbuf) == 0);

	// poke data into the endpoint
	lbuf->buffer[lbuf->head] = c;
	INC_HEAD(lbuf);
	if(move_line_start)
		lbuf->line_start = lbuf->head;
	if(was_empty && AVAILABLE_READ(lbuf) > 0)
		sem_release(lbuf->read_sem, 1);

	return 0;
}

int tty_ioctl(tty_desc *tty, int op, void *buf, size_t len)
{
	int err;

	mutex_lock(&tty->lock);

	TRACE(("TTY tty_ioctl start"));
	switch(op) {
		case _TTY_IOCTL_GET_TTY_NUM:
			err = tty->index;
			break;
		case _TTY_IOCTL_GET_TTY_FLAGS: {
			struct tty_flags flags;

			flags.input_flags = tty->buf[ENDPOINT_MASTER_WRITE].flags;
			flags.output_flags = tty->buf[ENDPOINT_SLAVE_WRITE].flags;

			err = user_memcpy(buf, &flags, sizeof(flags));
			if(err < 0)
				break;

			err = 0;
			break;
		}
		case _TTY_IOCTL_SET_TTY_FLAGS: {
			struct tty_flags flags;

			err = user_memcpy(&flags, buf, sizeof(flags));
			if(err < 0)
				break;

			tty->buf[ENDPOINT_MASTER_WRITE].flags = flags.input_flags;
			tty->buf[ENDPOINT_SLAVE_WRITE].flags = flags.output_flags;

			err = 0;
			break;
		}
		case _TTY_IOCTL_IS_A_TTY: {
			TRACE(("TTY tty_ioctl isatty"));
			err = 0;
			break;
		}
		default:
			TRACE(("TTY tty_ioctl default"));
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&tty->lock);

	return err;
}

ssize_t tty_read(tty_desc *tty, void *buf, ssize_t len, int endpoint)
{
	struct line_buffer *lbuf;
	ssize_t bytes_read = 0;
	ssize_t data_len;
	int err;

	if(len < 0)
		return ERR_INVALID_ARGS;

	if(len == 0)
		return 0;

	ASSERT(endpoint == ENDPOINT_MASTER_READ || endpoint == ENDPOINT_SLAVE_READ);
	lbuf = &tty->buf[endpoint];

	// wait for data in the buffer
	err = sem_acquire_etc(lbuf->read_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
	if(err == ERR_SEM_INTERRUPTED)
		return err;

	mutex_lock(&tty->lock);

	// quick sanity check
	ASSERT(lbuf->len > 0);
	ASSERT(lbuf->head < lbuf->len);
	ASSERT(lbuf->tail < lbuf->len);
	ASSERT(lbuf->line_start < lbuf->len);

	// figure out how much data is ready to be read
	data_len = AVAILABLE_READ(lbuf);
	len = min(data_len, len);

	ASSERT(len > 0);

	while(len > 0) {
		ssize_t copy_len = len;

		if(lbuf->tail + copy_len > lbuf->len) {
			copy_len = lbuf->len - lbuf->tail;
		}

		err = user_memcpy((char *)buf + bytes_read, lbuf->buffer + lbuf->tail, copy_len);
		if(err < 0) {
			sem_release(lbuf->read_sem, 1);
			goto err;
		}

		// update the buffer pointers
		lbuf->tail = (lbuf->tail + copy_len) % lbuf->len;
		len -= copy_len;
		bytes_read += copy_len;
	}

	// is there more data available?
	if(AVAILABLE_READ(lbuf) > 0)
		sem_release(lbuf->read_sem, 1);

	// did it used to be full?
	if(data_len == lbuf->len - 1)
		sem_release(lbuf->write_sem, 1);

err:
	mutex_unlock(&tty->lock);

	return bytes_read;
}



static ssize_t lbuf_write(char c, struct line_buffer* lbuf, bool canon);
static ssize_t lbuf_write(char c, struct line_buffer* lbuf, bool canon)
{
    TRACE(("TTY write: %c\r\n", c));
	
	if(c == '\n' && lbuf->flags & TTY_FLAG_NLCR) 
	{
		if(AVAILABLE_WRITE(lbuf) >= 2)
		{
			tty_insert_char(lbuf, '\r', false);
			tty_insert_char(lbuf, '\n', true);
			return 2;
		}
		return 0;
	}
	else if(c == '\r' && lbuf->flags & TTY_FLAG_CRNL) 
	{
		if(AVAILABLE_WRITE(lbuf) >= 2)
		{
			tty_insert_char(lbuf, '\r', false);
			tty_insert_char(lbuf, '\n', true);
			return 2;
		}
		return 0;
	}

	if(AVAILABLE_WRITE(lbuf) > (canon ? 2 : 0))
	{
        tty_insert_char(lbuf, c, !canon);
        return 1;
    }
    return 0;
}

ssize_t tty_write(tty_desc *tty, const void *buf, ssize_t len, int endpoint)
{
	struct line_buffer *lbuf_array[2];
	ssize_t buf_pos = 0;
	ssize_t bytes_written = 0;
	int err;
    bool acquired_sem_other_lbuf = false;

    if(len < 0)
		return ERR_INVALID_ARGS;

	if(len == 0)
		return 0;

	ASSERT(endpoint == ENDPOINT_MASTER_WRITE || endpoint == ENDPOINT_SLAVE_WRITE);
		
	lbuf_array[0] = &tty->buf[endpoint];
    lbuf_array[1] = &tty->buf[(endpoint == ENDPOINT_MASTER_WRITE) ? ENDPOINT_SLAVE_WRITE : ENDPOINT_MASTER_WRITE];

restart_loop:

    // wait on space in the circular buffer
    err = sem_acquire_etc(lbuf_array[0]->write_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
    if(err == ERR_SEM_INTERRUPTED)
        return err;

    if(lbuf_array[0]->flags & TTY_FLAG_ECHO) {
        err = sem_acquire_etc(lbuf_array[1]->write_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL);
        acquired_sem_other_lbuf = true;
        if(err == ERR_SEM_INTERRUPTED) {
            sem_release(lbuf_array[0]->write_sem, 1);
            return err;
        }
    }

    mutex_lock(&tty->lock);

	// quick sanity check
	ASSERT(lbuf_array[0]->len > 0);
	ASSERT(lbuf_array[0]->head < lbuf_array[0]->len);
	ASSERT(lbuf_array[0]->tail < lbuf_array[0]->len);
	ASSERT(lbuf_array[0]->line_start < lbuf_array[0]->len);

	while(buf_pos < len) {
	    bool echo;
		char c;
		int i;

        TRACE(("tty_write: regular loop: tty %p, lbuf %p, buf_pos %d, len %d\n", tty, lbuf_array[0], buf_pos, len));
		TRACE(("\tlbuf %p, flags 0x%x, head %d, tail %d, line_start %d\n", lbuf_array[0], lbuf_array[0]->flags, lbuf_array[0]->head, lbuf_array[0]->tail, lbuf_array[0]->line_start));

		// process this data one at a time
		err = user_memcpy(&c, (char *)buf + buf_pos, sizeof(c));	// XXX make this more efficient
		if(err < 0)
			goto exit_loop;
		buf_pos++; // advance to next character

		TRACE(("tty_write: char 0x%x\n", c));

		echo = true;
		for(i = 0; i < 2 && echo; i++)
		{
			echo = lbuf_array[i]->flags & TTY_FLAG_ECHO;
			if(lbuf_array[i]->flags & TTY_FLAG_CANON) 
			{
				// do line editing
				switch(c) 
				{
					case 0x08: // backspace
						// back the head pointer up one if it can
						if(lbuf_array[i]->head != lbuf_array[i]->line_start) 
						{
							bytes_written--;
							DEC_HEAD(lbuf_array[i]);
						}
						else
						{
							echo = false;
						}
						break;
					case 0:
						// eat it
						echo = false;
						break;
					default:
					{
						int bw = lbuf_write(c, lbuf_array[i], true);
						// stick it in the ring buffer
						bytes_written += bw;
						if(!bw)
						{
							echo = false;
						}
					}
				}
			} 
			else 
			{
				int bw = lbuf_write(c, lbuf_array[i], false);
				// The carriage return can be safely ignored in TTY_FLAG_NLCR.
				if(bw == 0)
				{
					buf_pos--;
					goto exit_loop;
				}
				else
				{
					bytes_written += bw;
				}
			}
		}
	}

exit_loop:
	mutex_unlock(&tty->lock);
    if(acquired_sem_other_lbuf) {
        sem_release(lbuf_array[1]->write_sem, 1);
    }
    sem_release(lbuf_array[0]->write_sem, 1);

    if(buf_pos < len)
        goto restart_loop;

	return bytes_written;
}

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	int i, j;
	int err;

	// setup the global tty structure
	memset(&thetty, 0, sizeof(thetty));
	err = mutex_init(&thetty.lock, "tty master lock");
	if(err < 0)
		panic("could not create master tty lock\n");

	// create device node
	devfs_publish_device("tty/master", NULL, &ttym_hooks);

	// set up the individual tty nodes
	for(i=0; i<NUM_TTYS; i++) {
		thetty.ttys[i].inuse = false;
		thetty.ttys[i].ref_count = 0;
		if(mutex_init(&thetty.ttys[i].lock, "tty lock") < 0)
			panic("couldn't create tty lock\n");

		// set up the two buffers (one for each direction)
		for(j=0; j<2; j++) {
			thetty.ttys[i].buf[j].read_sem = sem_create(0, "tty read sem");
			if(thetty.ttys[i].buf[j].read_sem < 0)
				panic("couldn't create tty read sem\n");
			thetty.ttys[i].buf[j].write_sem = sem_create(1, "tty write sem");
			if(thetty.ttys[i].buf[j].write_sem < 0)
				panic("couldn't create tty write sem\n");

			thetty.ttys[i].buf[j].head = 0;
			thetty.ttys[i].buf[j].tail = 0;
			thetty.ttys[i].buf[j].line_start = 0;
			thetty.ttys[i].buf[j].len = TTY_BUFFER_SIZE;
			thetty.ttys[i].buf[j].state = TTY_STATE_NORMAL;
			if(j == ENDPOINT_SLAVE_WRITE)
				thetty.ttys[i].buf[j].flags = TTY_FLAG_DEFAULT_OUTPUT; // slave writes to this one, translate LR to CRLF
			else if(j == ENDPOINT_MASTER_WRITE)
				thetty.ttys[i].buf[j].flags = TTY_FLAG_DEFAULT_INPUT; // master writes into this one. do line editing and echo back
		}

		thetty.ttys[i].index = devfs_publish_indexed_device("tty/slave", &thetty.ttys[i], &ttys_hooks);
	}

	return 0;
}

