/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STAGE1_H
#define _STAGE1_H


extern void stage1_main(void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned long vesa_ptr);
extern void clearscreen(void);
extern int dprintf(const char *fmt, ...);
extern void *kmalloc(unsigned int size);
extern void kfree(void *ptr);
extern int panic(const char *fmt, ...);


#endif

