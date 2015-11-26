/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <boot/shared/openfirmware.h>
#include "stage2_priv.h"
#include <stdarg.h>
#include <stdio.h>

static int of_input_handle = 0;
static int of_output_handle = 0;

int of_console_init(void)
{
    int chosen;

    /* open the input and output handle */
    chosen = of_finddevice("/chosen");
    of_getprop(chosen, "stdin", &of_input_handle, sizeof(of_input_handle));
    of_getprop(chosen, "stdout", &of_output_handle, sizeof(of_output_handle));

    return 0;
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
    while (*str) {
        if (*str == '\n')
            putchar('\r');
        putchar(*str);
        str++;
    }
}

void putchar(char c)
{
    if (of_output_handle != 0)
        of_write(of_output_handle, &c, 1);
}
#endif

