/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/stdarg.h>
#include <libc/printf.h>
#include "font.h"
#include "stage2_text.h"

static unsigned char *framebuffer;
static unsigned char draw_color;
static unsigned char back_color;
static int char_x,char_y;
static int screen_size_x, screen_size_y;
static int num_cols, num_rows;

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 12

static void s2_draw_char(unsigned char c, int x, int y)
{
	int i,j;
	unsigned char *base = &framebuffer[y*screen_size_x + x];
	unsigned char line;
	
	for(i=0; i<CHAR_HEIGHT; i++) {
		line = FONT[c*CHAR_HEIGHT + i];
		for(j=0; j<CHAR_WIDTH; j++) {
			base[j] = (line & 0x1) ? draw_color : back_color;
			line = line >> 1;
		}
		base += screen_size_x;
	}
}

int s2_printf(const char *fmt, ...)
{
	int ret;
	va_list args;
	char temp[256];

	va_start(args, fmt);
	ret = vsprintf(temp,fmt,args);
	va_end(args);

	s2_puts(temp);
	return ret;
}

void s2_puts(char *str)
{
	while(*str) {
		s2_putchar(*str);
		str++;
	}
}

void s2_putchar(char c)
{
	if(c == '\n') {
		char_x = 0;
		char_y++;
	} else {
		s2_draw_char(c, char_x * CHAR_WIDTH, char_y * CHAR_HEIGHT);
		char_x++;
	}
	if(char_x >= num_cols) {
		char_x = 0;
		char_y++;
	}
	if(char_y >= num_rows) {
		// scroll up
		memcpy(framebuffer, framebuffer + screen_size_x*CHAR_HEIGHT, screen_size_x * screen_size_y - screen_size_x*CHAR_HEIGHT);
		memset(framebuffer + (screen_size_y-CHAR_HEIGHT)*screen_size_x, back_color, screen_size_x*CHAR_HEIGHT);
		char_y--;
	}
 }

int s2_text_init()
{
	int i;

	framebuffer = (unsigned char *)0x96008000;
	screen_size_x = 1024;
	screen_size_y = 768;
	back_color = 0x0;
	draw_color = 0xff;
	char_x = char_y = 0;

	num_cols = screen_size_x / CHAR_WIDTH;
	num_rows = screen_size_y / CHAR_HEIGHT;	
	
	for(i = 0; i<screen_size_x * screen_size_y; i++) {
		framebuffer[i] = back_color;
	}

	return 0;
}

