/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
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

static unsigned char *heap_ptr = (unsigned char *)0x01000000; // 16MB

extern void *_payload_start;
extern void *_payload_end;
#define TARGET ((void *)0x400000) // 4MB

struct multiboot_info {
	uint32 flags;

	uint32 mem_lower;
	uint32 mem_upper;

	uint32 boot_device;

	uint32 cmdline;

	uint32 mods_count;
	uint32 mods_addr;

	uint32 syms[4];

	uint32 mmap_length;
	uint32 mmap_addr;
	
	uint32 drives_length;
	uint32 drives_addr;

	uint32 config_table;

	uint32 boot_loader_name;

	uint32 apm_table;

	uint32 vbe_control_info;
	uint32 vbe_mode_info;
	uint32 vbe_mode;
	uint32 vbe_interface_seg;
	uint32 vbe_interface_off;
	uint32 vbe_interface_len;
};

void stage1_main(void *multiboot_info);

static uint32 read32(const void *ptr, unsigned int offset)
{
	return *(const uint32 *)((const char *)ptr + offset);
}

static void dump_multiboot(const void *multiboot)
{
	uint32 flags = read32(multiboot, 0);

	dprintf("flags 0x%x\n", flags);

	if (flags & (1<<0)) {
		dprintf("mem_lower 0x%x\n", read32(multiboot, 4));
		dprintf("mem_upper 0x%x\n", read32(multiboot, 8));
	}
	if (flags & (1<<1)) {
		dprintf("boot_device 0x%x\n", read32(multiboot, 12));
	}
	if (flags & (1<<2)) {
		dprintf("cmdline 0x%x\n", read32(multiboot, 16));
	}
	if (flags & (1<<3)) {
		dprintf("mods_count 0x%x\n", read32(multiboot, 20));
		dprintf("mods_addr 0x%x\n", read32(multiboot, 24));
	}
	if (flags & (1<<6)) {
		dprintf("mmap_length 0x%x\n", read32(multiboot, 44));
		dprintf("mmap_addr 0x%x\n", read32(multiboot, 48));
	}

}

//void stage1_main(void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned long vesa_ptr)
void stage1_main(void *multiboot_info)
{
	unsigned long len;
	boot_dir *bootdir = TARGET;
	void (*stage2_entry)(void *ext_mem_block, int ext_mem_count, int in_vesa, unsigned int vesa_ptr, int console_ptr);

	clearscreen();

	dprintf("stage1 boot\n");
	dprintf("multiboot info %p\n", multiboot_info);
	dump_multiboot(multiboot_info);

	dprintf("%lld\n", 0x123456789abcdefLL);

	dprintf("decompressing system, payload at %p...\n", &_payload_start);

	len = gunzip((unsigned char const *)&_payload_start, TARGET, kmalloc(32*1024));
	dprintf("done, len %d\n", len);


#if 1
	dprintf("finding stage2...");
	stage2_entry = (void*)((char *)bootdir + bootdir->bd_entry[1].be_offset * PAGE_SIZE + bootdir->bd_entry[1].be_code_ventr);
	dprintf("entry at %p\n", stage2_entry);

	// jump into stage2
	stage2_entry(0,0,0,0,0);
#endif
}

void *kmalloc(unsigned int size)
{
	dprintf("kmalloc: size %d, ptr %p\n", size, heap_ptr - size);

	return (heap_ptr -= size);
}

void kfree(void *ptr)
{
}

void clearscreen()
{
	int i;

	for(i=0; i< SCREEN_WIDTH*SCREEN_HEIGHT*2; i++) {
		kScreenBase[i] = 0xf20;
	}
}

static void scrup()
{
	int i;
	memcpy(kScreenBase, kScreenBase + SCREEN_WIDTH,
		SCREEN_WIDTH * SCREEN_HEIGHT * 2 - SCREEN_WIDTH * 2);
	screenOffset = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
	for(i=0; i<SCREEN_WIDTH; i++)
		kScreenBase[screenOffset + i] = 0x0720;
	line = SCREEN_HEIGHT - 1;
}

int puts(const char *str)
{
	while (*str) {
		if (*str == '\n') {
			line++;
			if(line > SCREEN_HEIGHT - 1)
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
	for(;;);
	return ret;
}

