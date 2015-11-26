/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <boot/stage2.h>
#include <boot/bootdir.h>
#include "stage1.h"
#include "inflate.h"

// needed for console stuff
static unsigned short *kScreenBase = (unsigned short*) 0xb8000;
static unsigned screenOffset = 0;
static unsigned int line = 0;

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define PAGE_SIZE 4096

static unsigned char *heap_ptr = (unsigned char *)0x1000000;

extern void *_end;
#define TARGET ((void *)0x400000)

__SECTION(".text.boot")
void _start(unsigned int mem, void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned int vesa_ptr)
{
    unsigned long len;
    boot_dir *bootdir = TARGET;
    void (*stage2_entry)(unsigned int mem, void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned int vesa_ptr, int console_ptr);

    clearscreen();

    dprintf("stage1 boot, decompressing system");

    dprintf("stage1: decompressing from %p\n", &_end);
    len = gunzip((unsigned char const *)&_end, TARGET, kmalloc(32*1024));
    dprintf("done, len %d\n", len);

    dprintf("finding stage2...");
    stage2_entry = (void*)(bootdir->bd_entry[1].be_code_ventr);
    dprintf("entry at %p\n", stage2_entry);

    // jump into stage2
    stage2_entry(mem, ext_mem_block, ext_mem_count, in_vesa, vesa_ptr, screenOffset);
}

void *kmalloc(unsigned int size)
{
//  dprintf("kmalloc: size %d, ptr %p\n", size, heap_ptr - size);

    return (heap_ptr -= size);
}

void kfree(void *ptr)
{
}

void clearscreen()
{
    int i;

    for (i=0; i< SCREEN_WIDTH*SCREEN_HEIGHT*2; i++) {
        kScreenBase[i] = 0xf20;
    }
}

static void scrup()
{
    int i;
    memcpy(kScreenBase, kScreenBase + SCREEN_WIDTH,
           SCREEN_WIDTH * SCREEN_HEIGHT * 2 - SCREEN_WIDTH * 2);
    screenOffset = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
    for (i=0; i<SCREEN_WIDTH; i++)
        kScreenBase[screenOffset + i] = 0x0720;
    line = SCREEN_HEIGHT - 1;
}

int puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            line++;
            if (line > SCREEN_HEIGHT - 1)
                scrup();
            else
                screenOffset += SCREEN_WIDTH - (screenOffset % 80);
        } else {
            kScreenBase[screenOffset++] = 0xf00 | *str;
        }
        if (screenOffset >= SCREEN_WIDTH * SCREEN_HEIGHT)
            scrup();

        str++;
    }
    return 0;
}

int dprintf(const char *fmt, ...)
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


int panic(const char *fmt, ...)
{
    int ret;
    va_list args;
    char temp[256];

    va_start(args, fmt);
    ret = vsprintf(temp,fmt,args);
    va_end(args);

    puts("STAGE1 PANIC: ");
    puts(temp);
    puts("\n");

    puts("spinning forever...");
    for (;;);
    return ret;
}

