/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include "stage2_priv.h"
#include <stdarg.h>
#include <stdio.h>

static int (*of)(void *) = 0; // openfirmware entry
static int of_input_handle = 0;
static int of_output_handle = 0;

int of_init(void *of_entry)
{
	int chosen;

	of = of_entry;

	/* open the input and output handle */
	chosen = of_finddevice("/chosen");
	of_getprop(chosen, "stdin", &of_input_handle, sizeof(of_input_handle));
	of_getprop(chosen, "stdout", &of_output_handle, sizeof(of_output_handle));

	return 0;
}

int of_open(const char *node_name)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		const char *node_name;
		int handle;
	} args;

	args.name = "open";
	args.num_args = 1;
	args.num_returns = 1;
	args.node_name = node_name;

	of(&args);

	return args.handle;
}

int of_finddevice(const char *dev)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		const char *device;
		int handle;
	} args;

	args.name = "finddevice";
	args.num_args = 1;
	args.num_returns = 1;
	args.device = dev;

	of(&args);

	return args.handle;
}

int of_instance_to_package(int in_handle)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int in_handle;
		int handle;
	} args;

	args.name = "instance-to-package";
	args.num_args = 1;
	args.num_returns = 1;
	args.in_handle = in_handle;

	of(&args);

	return args.handle;
}

int of_getprop(int handle, const char *prop, void *buf, int buf_len)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int handle;
		const char *prop;
		void *buf;
		int buf_len;
		int size;
	} args;

	args.name = "getprop";
	args.num_args = 4;
	args.num_returns = 1;
	args.handle = handle;
	args.prop = prop;
	args.buf = buf;
	args.buf_len = buf_len;

	of(&args);

	return args.size;
}

int of_setprop(int handle, const char *prop, const void *buf, int buf_len)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int handle;
		const char *prop;
		const void *buf;
		int buf_len;
		int size;
	} args;

	args.name = "setprop";
	args.num_args = 4;
	args.num_returns = 1;
	args.handle = handle;
	args.prop = prop;
	args.buf = buf;
	args.buf_len = buf_len;

	of(&args);

	return args.size;
}

int of_read(int handle, void *buf, int buf_len)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int handle;
		void *buf;
		int buf_len;
		int size;
	} args;

	args.name = "read";
	args.num_args = 3;
	args.num_returns = 1;
	args.handle = handle;
	args.buf = buf;
	args.buf_len = buf_len;

	of(&args);

	return args.size;
}

int of_write(int handle, void *buf, int buf_len)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int handle;
		void *buf;
		int buf_len;
		int size;
	} args;

	args.name = "write";
	args.num_args = 3;
	args.num_returns = 1;
	args.handle = handle;
	args.buf = buf;
	args.buf_len = buf_len;

	of(&args);

	return args.size;
}

int of_seek(int handle, long long pos)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		int handle;
		long long pos;
		int status;
	} args;

	args.name= "seek";
	args.num_args = 3;
	args.num_returns = 1;
	args.handle = handle;
	args.pos = pos;

	of(&args);

	return args.status;
}

void of_exit(void)
{
	struct {
		const char *name;
	} args;

	for (;;) {
		args.name = "exit";
		of(&args);;
	}
}

void *of_claim(unsigned int vaddr, unsigned int size, unsigned int align)
{
	struct {
		const char *name;
		int num_args;
		int num_returns;
		unsigned int vaddr;
		unsigned int size;
		unsigned int align;
		void *ret;
	} args;

	args.name = "claim";
	args.num_args = 3;
	args.num_returns = 1;
	args.vaddr = vaddr;
	args.size = size;
	args.align = align;

	of(&args);

	return args.ret;
}

#if 1
int printf(const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	puts(temp);
	return ret;
}

void puts(char *str)
{
	while(*str) {
		if(*str == '\n')
			putchar('\r');
		putchar(*str);
		str++;
	}
}

void putchar(char c)
{
	if(of_output_handle != 0)
		of_write(of_output_handle, &c, 1);
}
#endif

