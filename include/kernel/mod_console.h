/*
** Copyright 2002, Trey Boudreau. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _KERNEL_MOD_CONSOLE_H
#define _KERNEL_MOD_CONSOLE_H

#include <kernel/module.h>

typedef struct {
    int (*get_size)(int *width, int *height);
    int (*move_cursor)(int x, int y);
    int (*put_glyph)(int x, int y, uint8 glyph, uint8 attr);
    int (*fill_glyph)(int x, int y, int width, int height, uint8 glyph, uint8 attr);
    int (*blt)(int srcx, int srcy, int width, int height, int destx, int desty);
} mod_console;

#endif
