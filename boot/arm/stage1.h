/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STAGE1_H
#define _STAGE1_H

#define LED_DISPLAY(pos, c) \
    (*(char *)(0x08000100 + (3-pos)*2) = (c))

extern void _start(unsigned int mem, void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned int vesa_ptr);
extern void clearscreen(void);
extern void puts(const char *str);
extern int dprintf(const char *fmt, ...);
extern void *kmalloc(unsigned int size);
extern void kfree(void *ptr);
extern int panic(const char *fmt, ...);


#endif

