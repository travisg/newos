/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/smp.h>
#include <kernel/arch/int.h>
#include <sys/errors.h>
#include <boot/stage2.h>
#include <nulibc/string.h>
#include <nulibc/stdio.h>

#define NUM_IO_HANDLERS 256

struct io_handler {
	struct io_handler *next;
	int (*func)(void*);
	void* data;
};

static struct io_handler **io_handlers = NULL;
static spinlock_t int_handler_list_spinlock = 0;

int int_init(kernel_args *ka)
{
	dprintf("init_int_handlers: entry\n");

	int_handler_list_spinlock = 0;

	return arch_int_init(ka);
}

int int_init2(kernel_args *ka)
{
	io_handlers = (struct io_handler **)kmalloc(sizeof(struct io_handler *) * NUM_IO_HANDLERS);
	if(io_handlers == NULL)
		panic("int_init2: could not create io handler table!\n");

	memset(io_handlers, 0, sizeof(struct io_handler *) * NUM_IO_HANDLERS);

	return arch_int_init2(ka);
}

int int_set_io_interrupt_handler(int vector, int (*func)(void*), void* data)
{
	struct io_handler *io;
	int state;

	// insert this io handler in the chain of interrupt
	// handlers registered for this io interrupt

	io = (struct io_handler *)kmalloc(sizeof(struct io_handler));
	if(io == NULL)
		return ERR_NO_MEMORY;
	io->func = func;
	io->data = data;

	state = int_disable_interrupts();
	acquire_spinlock(&int_handler_list_spinlock);
	io->next = io_handlers[vector];
	io_handlers[vector] = io;
	release_spinlock(&int_handler_list_spinlock);
	int_restore_interrupts(state);

	arch_int_enable_io_interrupt(vector);

	return NO_ERROR;
}

int int_remove_io_interrupt_handler(int vector, int (*func)(void*), void* data)
{
	struct io_handler *io, *prev = NULL;
	int state;

	// lock the structures down so it is not modified while we search
	state = int_disable_interrupts();
	acquire_spinlock(&int_handler_list_spinlock);

	// start at the beginning
	io = io_handlers[vector];

	// while not at end
	while(io != NULL) {
		// see if we match both the function & data
		if (io->func == func && io->data == data)
			break;

		// Store our backlink and move to next
		prev = io;
		io = io->next;
	}

	// If we found it
	if (io != NULL) {
		// unlink it, taking care of the change it was the first in line
		if (prev != NULL)
			prev->next = io->next;
		else
			io_handlers[vector] = io->next;
	}

	// release our lock as we're done with the table
	release_spinlock(&int_handler_list_spinlock);
	int_restore_interrupts(state);

	// and disable the IRQ if nothing left
	if (io != NULL) {
		if (prev == NULL && io->next == NULL)
			arch_int_disable_io_interrupt(vector);

		kfree(io);
	}

	return (io != NULL) ? NO_ERROR : ERR_INVALID_ARGS;
}

int int_io_interrupt_handler(int vector)
{
	int ret = INT_NO_RESCHEDULE;

	// XXX rare race condition here. The io_handlers list is not locked
	// need to find a good way to solve that problem

	if(io_handlers[vector] == NULL) {
		dprintf("unhandled io interrupt %d\n", vector);
	} else {
		struct io_handler *io;
		int temp_ret;

		io = io_handlers[vector];
		while(io != NULL) {
			temp_ret = io->func(io->data);
			if(temp_ret == INT_RESCHEDULE)
				ret = INT_RESCHEDULE;
			io = io->next;
		}
	}

	return ret;
}

