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
#include <libc/string.h>
#include <libc/printf.h>

#define NUM_IO_HANDLERS 256

struct io_handler {
	struct io_handler *next;
	int (*func)(void);
};

static struct io_handler **io_handlers = NULL;
static int int_handler_list_spinlock = 0;

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

int int_set_io_interrupt_handler(int vector, int (*func)(void))
{
	struct io_handler *io;
	
	// insert this io handler in the chain of interrupt
	// handlers registered for this io interrupt

	io = (struct io_handler *)kmalloc(sizeof(struct io_handler));
	if(io == NULL)
		return ERR_NO_MEMORY;
	io->func = func;

	acquire_spinlock(&int_handler_list_spinlock);
	io->next = io_handlers[vector];
	io_handlers[vector] = io;
	release_spinlock(&int_handler_list_spinlock);

	arch_int_enable_io_interrupt(vector);

	return NO_ERROR;
}

int int_io_interrupt_handler(int vector)
{
	int ret = INT_NO_RESCHEDULE;

	if(io_handlers[vector] == NULL) {
		dprintf("unhandled io interrupt %d\n", vector);
	} else {
		struct io_handler *io;
		int temp_ret;
		
		io = io_handlers[vector];
		while(io != NULL) {
			temp_ret = io->func();
			if(temp_ret == INT_RESCHEDULE)
				ret = INT_RESCHEDULE;
			io = io->next;
		}
	}

	return ret;
}

