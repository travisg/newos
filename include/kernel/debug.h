/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_DEBUG_H
#define _KERNEL_DEBUG_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <sys/cdefs.h>
#include <kernel/cpu.h>
#include <kernel/arch/debug.h>

extern int dbg_register_file[GDB_REGISTER_FILE_COUNT];

int dbg_init(kernel_args *ka);
int dbg_init2(kernel_args *ka);
char dbg_putch(char c);
void dbg_puts(const char *s);
ssize_t dbg_write(const void *buf, ssize_t len);
bool dbg_set_serial_debug(bool new_val);
bool dbg_get_serial_debug(void);
int dprintf(const char *fmt, ...) __PRINTFLIKE(1,2);
int panic(const char *fmt, ...) __PRINTFLIKE(1,2);
void kernel_debugger(void);
int dbg_add_command(void (*func)(int, char **), const char *cmd, const char *desc);

/* arch provided */
extern void dbg_save_registers(int *);
extern void dbg_make_register_file(unsigned int *file, const struct iframe *frame);

#if _ASSERT_LEVEL >= 1
#define ASSERT(x) \
	{ if(!(x)) panic("ASSERT FAILED (%s:%d): %s\n", __FILE__, __LINE__, #x); }
#else
#define ASSERT(x)
#endif

#if _ASSERT_LEVEL >= 2
#define ASSERT2(x) \
	{ if(!(x)) panic("ASSERT2 FAILED (%s:%d): %s\n", __FILE__, __LINE__, #x); }
#else
#define ASSERT2(x)
#endif

#define PANIC_UNIMPLEMENTED() \
	panic("%s: unimplemented\n", __FUNCTION__)
#define ATRACE(x) \
	dprintf("TRACE: (%s:%s:%d) %s", __FILE__, __FUNCTION__, __LINE__, x)

#endif

