/*
** Copyright 2001-2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <boot/stage2.h>
#include "stage2_priv.h"
#include "inflate.h"

extern void *_payload_start;
extern void *_payload_end;
#define TARGET ((void *)0x400000) // 4MB

void stage1_main(void *multiboot_info);

void stage1_main(void *multiboot_info)
{
    unsigned long len;

    clearscreen();

    dprintf("stage1 boot\n");

    dprintf("decompressing system, payload at %p...\n", &_payload_start);
    len = gunzip((unsigned char const *)&_payload_start, TARGET, kmalloc(32*1024));

    stage2_main(multiboot_info, 0, 0, 0);
}

