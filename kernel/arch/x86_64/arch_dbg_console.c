/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/*
** Modified 2001/09/05 by Rob Judd<judd@ob-wan.com>
*/
#include <kernel/kernel.h>
#include <kernel/int.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/dbg_console.h>

#include <boot/stage2.h>

#include <string.h>

#if _SERIAL_DBG_PORT == 1
static const int serial_ioport_base = 0x3f8;
#elif _SERIAL_DBG_PORT == 2
static const int serial_ioport_base = 0x2f8;
#elif _SERIAL_DBG_PORT == 0xe9
#define BOCHS_E9_HACK 1
#else
#error _SERIAL_DBG_PORT set to unsupported com port
#endif

static const int dbg_baud_rate = 115200;

int arch_dbg_con_init(kernel_args *ka)
{
#if !BOCHS_E9_HACK
    short divisor = 115200 / dbg_baud_rate;

    out8(0x80, serial_ioport_base+3);   /* set up to load divisor latch */
    out8(divisor & 0xf, serial_ioport_base);        /* LSB */
    out8(divisor >> 8, serial_ioport_base+1);       /* MSB */
    out8(3, serial_ioport_base+3);      /* 8N1 */
#endif
    return NO_ERROR;
}

int arch_dbg_con_init2(kernel_args *ka)
{
    return NO_ERROR;
}

char arch_dbg_con_read(void)
{
#if BOCHS_E9_HACK
    return in8(0xe9);
#else
    while ((in8(serial_ioport_base+5) & 1) == 0)
        ;
    return in8(serial_ioport_base);
#endif
}

static void _arch_dbg_con_putch(const char c)
{
#if BOCHS_E9_HACK
    out8(c, 0xe9);
#else

    while ((in8(serial_ioport_base+5) & 0x20) == 0)
        ;
    out8(c, serial_ioport_base);

#endif
}

char arch_dbg_con_putch(const char c)
{
    if (c == '\n') {
        _arch_dbg_con_putch('\r');
        _arch_dbg_con_putch('\n');
    } else if (c != '\r')
        _arch_dbg_con_putch(c);

    return c;
}

void arch_dbg_con_puts(const char *s)
{
    while (*s != '\0') {
        arch_dbg_con_putch(*s);
        s++;
    }
}

ssize_t arch_dbg_con_write(const void *buf, ssize_t len)
{
    const char *cbuf = (const char *)buf;
    ssize_t ret = len;

    while (len > 0) {
        arch_dbg_con_putch(*cbuf);
        cbuf++;
        len--;
    }
    return ret;
}

