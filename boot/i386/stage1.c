#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <boot/stage2.h>
#include <boot/bootdir.h>
#include "inflate.h"

// needed for console stuff
static unsigned short *kScreenBase = (unsigned short*) 0xb8000;
static unsigned screenOffset = 0;
static unsigned int line = 0;

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define PAGE_SIZE 4096

extern void _start(unsigned int mem, int in_vesa, unsigned int vesa_ptr);
extern void clearscreen(void);
extern void puts(const char *str);
extern int dprintf(const char *fmt, ...);
void *kmalloc(int size);
void kfree(void *ptr);

static unsigned char *heap_ptr = 0x1000000;

extern void *_end;
#define TARGET ((void *)0x400000)

void _start(unsigned int mem, int in_vesa, unsigned int vesa_ptr)
{
	unsigned long len;
	boot_dir *bootdir = TARGET;
	void (*stage2_entry)(unsigned int mem, int in_vesa, unsigned int vesa_ptr, int console_ptr);

	clearscreen();

	dprintf("stage1 boot, decompressing system");

	len = gunzip(&_end, TARGET, kmalloc(32*1024));
	dprintf("done, len %d\n", len);

	dprintf("finding stage2...");
	stage2_entry = ((char *)bootdir + bootdir->bd_entry[1].be_offset * PAGE_SIZE + bootdir->bd_entry[1].be_code_ventr);
	dprintf("entry at %p\n", stage2_entry);

	// jump into stage2
	stage2_entry(mem, in_vesa, vesa_ptr, screenOffset);
}

void *kmalloc(int size)
{
//	dprintf("kmalloc: size %d, ptr %p\n", size, heap_ptr - size);

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

void puts(const char *str)
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
		if (screenOffset > SCREEN_WIDTH * SCREEN_HEIGHT)
			scrup();

		str++;
	}
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

	puts("spinning forever...");
	for(;;);
	return ret;
}

