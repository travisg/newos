/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** VMWare support Copyright 2002, Trey Boudreau. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <newos/errors.h>
#include <kernel/bus/bus.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <string.h>
#include <stdio.h>

#include <kernel/dev/arch/i386/console/console_dev.h>
#include "svga_reg.h"
#include "vm_device_version.h"
#include "font.h"


static struct {
	addr		fb_base;
	addr		fb_phys_base;
	addr		fb_size;
	addr		fifo_base;
	addr		fifo_phys_base;
	addr		fifo_size;
	uint32		scrn_width;
	uint32		scrn_height;
	uint32		fb_offset;
	uint32		bytes_per_line;
	uint32		bits_per_pixel;
	uint32		red_mask;
	uint32		blue_mask;
	uint32		green_mask;
	int32		cursor_x;
	int32		cursor_y;
	int32		text_rows;
	int32		text_cols;
	uint32		glyph_width;
	uint32		glyph_height;
	uint32		fg, bg;
	int32		cursor_visible;
	region_id	fb_region;
	region_id	fifo_region;
	uint16		ports;
} vcons;

static uint8 bit_reversed[256];

static void show_cursor(int show);
static void init_cursor();
static void setup_bit_reversed(void);
static void write_glyph(char c, uint32 x, uint32 y, uint32 fg, uint32 bg);
static int set_mode(uint32 width, uint32 height);
static void load_font();
static int find_and_map(void);
void out_reg(uint32 index, uint32 value);
uint32 in_reg(uint32 index);
static void init_fifo();
static void writeFIFO(uint32 value);
static char console_putch(const char c);
static void clear_screen();
static void fill_rect(uint32 color, uint32 x, uint32 y, uint32 width, uint32 height);
static void copy_rect(uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height);
static void invert_rect(uint32 x, uint32 y, uint32 width, uint32 height);
static void define_font(uint32 id, uint32 width, uint32 height, uint8 *bits);
static void copy_bitmap(uint32 id, uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height, uint32 fg, uint32 bg);

#define SCREEN_START 0xb8000
#define SCREEN_END   0xc0000
#define LINES 25
#define COLUMNS 80
#define NPAR 16
#define TAB_SIZE 8
#define TAB_MASK 7

#define TEXT_INDEX 0x3d4
#define TEXT_DATA  0x3d5

#define TEXT_CURSOR_LO 0x0f
#define TEXT_CURSOR_HI 0x0e

static mutex console_lock;
static int keyboard_fd = -1;

static void setup_bit_reversed(void)
{
	uint8 val, rev;
	int i, j;
	for (i = 0; i < 256; i++)
	{
		val = i;
		rev = 0;
		for (j = 0; j < 8; j++)
		{
			rev <<= 1;
			if (val & 1) rev |= 1;
			val >>= 1;
		}
		bit_reversed[i] = rev;
	}
}

static void init_fifo()
{
	uint32* vmwareFIFO = (uint32*)vcons.fifo_base;
	vmwareFIFO[SVGA_FIFO_MIN] = 16;
	vmwareFIFO[SVGA_FIFO_MAX] = vcons.fifo_size;
	vmwareFIFO[SVGA_FIFO_NEXT_CMD] = 16;
	vmwareFIFO[SVGA_FIFO_STOP] = 16;
	out_reg(SVGA_REG_CONFIG_DONE, 1);
}

static void
writeFIFO(uint32 value)
{
	uint32* vmwareFIFO = (uint32*)vcons.fifo_base;

	/* Need to sync? */
	if ((vmwareFIFO[SVGA_FIFO_NEXT_CMD] + sizeof(uint32) == vmwareFIFO[SVGA_FIFO_STOP])
		|| (vmwareFIFO[SVGA_FIFO_NEXT_CMD] == vmwareFIFO[SVGA_FIFO_MAX] - sizeof(uint32) &&
		vmwareFIFO[SVGA_FIFO_STOP] == vmwareFIFO[SVGA_FIFO_MIN]))
	{
		out_reg(SVGA_REG_SYNC, 1);
		while (in_reg(SVGA_REG_BUSY)) ;
	}
	vmwareFIFO[vmwareFIFO[SVGA_FIFO_NEXT_CMD] / sizeof(uint32)] = value;
	vmwareFIFO[SVGA_FIFO_NEXT_CMD] += sizeof(uint32);
	if (vmwareFIFO[SVGA_FIFO_NEXT_CMD] == vmwareFIFO[SVGA_FIFO_MAX])
	{
		vmwareFIFO[SVGA_FIFO_NEXT_CMD] = vmwareFIFO[SVGA_FIFO_MIN];
	}
}

static void clear_screen()
{
	fill_rect(vcons.bg, 0, 0, vcons.scrn_width, vcons.scrn_height);
}

#if 0
static void init_cursor()
{
	unsigned int i;
	writeFIFO(SVGA_CMD_DEFINE_CURSOR);
	writeFIFO(4); // ID
	writeFIFO(0); // hot-x
	writeFIFO(0); // hot-y
	writeFIFO(vcons.glyph_width);
	writeFIFO(vcons.glyph_height);
	writeFIFO(vcons.bits_per_pixel); // source bits per pixel
	writeFIFO(vcons.bits_per_pixel); // mask bits per pixel
	for (i = 0; i < vcons.glyph_height * vcons.glyph_width; i++) writeFIFO(0);
	for (i = 0; i < vcons.glyph_height * vcons.glyph_width; i++) writeFIFO(~0);
}
#endif

static void show_cursor(int show)
{
	show = show != 0;
#if 0
	writeFIFO(SVGA_CMD_DISPLAY_CURSOR);
	writeFIFO(4);
	writeFIFO(show ? 1 : 0);
#else
	if (show == vcons.cursor_visible) return;
	vcons.cursor_visible = show;
	invert_rect(vcons.cursor_x * vcons.glyph_width, vcons.cursor_y * vcons.glyph_height, vcons.glyph_width, vcons.glyph_height);
#endif
}

static void update_cursor(int x, int y)
{
#if 0
	writeFIFO(SVGA_CMD_MOVE_CURSOR);
	writeFIFO(x * vcons.glyph_width);
	writeFIFO(y * vcons.glyph_height);
	dprintf("update_cursor(%d,%d)\n", x, y);
#endif
}

static void invert_rect(uint32 x, uint32 y, uint32 width, uint32 height)
{
#define GXinvert 0xa
	writeFIFO(SVGA_CMD_RECT_ROP_FILL);
	writeFIFO(0); // color
	writeFIFO(x);
	writeFIFO(y);
	writeFIFO(width);
	writeFIFO(height);
	writeFIFO(GXinvert);
}

static void copy_rect(uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height)
{
	writeFIFO(SVGA_CMD_RECT_COPY);
	writeFIFO(src_x);
	writeFIFO(src_y);
	writeFIFO(dst_x);
	writeFIFO(dst_y);
	writeFIFO(width);
	writeFIFO(height);
}

static void fill_rect(uint32 color, uint32 x, uint32 y, uint32 width, uint32 height)
{
	writeFIFO(SVGA_CMD_RECT_FILL);
	writeFIFO(color);
	writeFIFO(x);
	writeFIFO(y);
	writeFIFO(width);
	writeFIFO(height);
	//dprintf("fill_rect(%6x,%u,%u,%u,%u)\n", color, x, y, width, height);
}

static void gotoxy(int new_x,int new_y)
{
	dprintf("gotoxy(%d,%d)\n", new_x, new_y);
	if (new_x >= vcons.text_cols || new_y >= vcons.text_rows)
		return;
	vcons.cursor_x = new_x;
	vcons.cursor_y = new_y;
}

static void scrup(void)
{
	unsigned long i;

	// move the screen up one
	copy_rect(0, vcons.glyph_height, 0, 0, vcons.scrn_width, vcons.scrn_height - vcons.glyph_height);

	// set the new position to the beginning of the last line

	// clear the bottom line
	fill_rect(vcons.bg, 0, vcons.scrn_height - vcons.glyph_height, vcons.scrn_width, vcons.glyph_height);
}

static void lf(void)
{
	dprintf("lf\n");
	if (vcons.cursor_y+1 < vcons.text_rows) {
		vcons.cursor_y++;
		return;
	}
	scrup();
}

static void cr(void)
{
	vcons.cursor_x = 0;
	dprintf("cr\n");
}

static void del(void)
{
	if (vcons.cursor_x > 0) {
		vcons.cursor_x--;
		console_putch(' ');
		vcons.cursor_x--;
	}
	dprintf("del\n");
}

static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
	saved_x=vcons.cursor_x;
	saved_y=vcons.cursor_y;
}

static void restore_cur(void)
{
	vcons.cursor_x=saved_x;
	vcons.cursor_y=saved_y;
}

static char console_putch(const char c)
{
	if(++vcons.cursor_x >= vcons.text_cols) {
		cr();
		lf();
	}

	// blit the char
	write_glyph(c, 
		(vcons.cursor_x-1) * vcons.glyph_width,
		vcons.cursor_y * vcons.glyph_height,
		vcons.fg, vcons.bg);

	// advance the cursor
	return c;
}

static void tab(void)
{
	vcons.cursor_x = (vcons.cursor_x + TAB_SIZE) & ~TAB_MASK;
	if (vcons.cursor_x >= vcons.text_cols) {
		vcons.cursor_x -= vcons.text_cols;
		lf();
	}
}

static int console_open(dev_ident ident, dev_cookie *cookie)
{
	return 0;
}

static int console_freecookie(dev_cookie cookie)
{
	return 0;
}

static int console_seek(dev_cookie cookie, off_t pos, seek_type st)
{
//	dprintf("console_seek: entry\n");

	return ERR_NOT_ALLOWED;
}

static int console_close(dev_cookie cookie)
{
//	dprintf("console_close: entry\n");

	return 0;
}

static ssize_t console_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	return sys_read(keyboard_fd, buf, 0, len);
}

static ssize_t _console_write(const void *buf, size_t len)
{
	size_t i;
	const char *c;

	for(i=0; i<len; i++) {
		c = &((const char *)buf)[i];
		switch(*c) {
			case '\n':
				cr();
				lf();
				break;
			case '\r':
				cr();
				break;
			case 0x8: // backspace
				del();
				break;
			case '\t':
				tab();
				break;
			case '\0':
				break;
			default:
				console_putch(*c);
		}
	}
	return len;
}

static ssize_t console_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	ssize_t err;

//	dprintf("console_write: entry, len = %d\n", *len);

	mutex_lock(&console_lock);

	show_cursor(0);
	err = _console_write(buf, len);
	show_cursor(1);
	//update_cursor(vcons.cursor_x, vcons.cursor_y);

	mutex_unlock(&console_lock);

	return err;
}

static int console_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	int err;

	switch(op) {
		case CONSOLE_OP_WRITEXY: {
			int x,y;
			mutex_lock(&console_lock);

			x = ((int *)buf)[0];
			y = ((int *)buf)[1];

			show_cursor(0);
			save_cur();
			gotoxy(x, y);
			if(_console_write(((char *)buf) + 2*sizeof(int), len - 2*sizeof(int)) > 0)
				err = 0; // we're okay
			else
				err = ERR_IO_ERROR;
			restore_cur();
			show_cursor(1);
			mutex_unlock(&console_lock);
			break;
		}
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static struct dev_calls console_hooks = {
	&console_open,
	&console_close,
	&console_freecookie,
	&console_seek,
	&console_ioctl,
	&console_read,
	&console_write,
	/* cannot page from /dev/console */
	NULL,
	NULL,
	NULL
};

#define MIN(a, b) ((a)<(b)?(a):(b))

void out_reg(uint32 index, uint32 value)
{
	//out32(index, vcons.ports + (SVGA_INDEX_PORT << 2));
	//out32(value, vcons.ports + (SVGA_VALUE_PORT << 2));
	out32(index, vcons.ports + SVGA_INDEX_PORT);
	out32(value, vcons.ports + SVGA_VALUE_PORT);
}

uint32 in_reg(uint32 index)
{
	//out32(index, vcons.ports + (SVGA_INDEX_PORT << 2));
	//return in32(vcons.ports + (SVGA_VALUE_PORT << 2));
	out32(index, vcons.ports + SVGA_INDEX_PORT);
	return in32(vcons.ports + SVGA_VALUE_PORT);
}

static void define_font(uint32 id, uint32 width, uint32 height, uint8 *bits)
{
	int words;
	dprintf("define_font(%u, %u,%u, %p)\n", id, width, height, bits);
	writeFIFO(SVGA_CMD_DEFINE_BITMAP);
	writeFIFO(id);
	writeFIFO(width);
	writeFIFO(height);
	//words = (((width+7) >> 3) * height) >> 2;
	while (height--) writeFIFO(bit_reversed[*bits++]);
}

static void copy_bitmap(uint32 id, uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height, uint32 fg, uint32 bg)
{
	writeFIFO(SVGA_CMD_RECT_BITMAP_COPY);
	writeFIFO(id);
	writeFIFO(src_x);
	writeFIFO(src_y);
	writeFIFO(dst_x);
	writeFIFO(dst_y);
	writeFIFO(width);
	writeFIFO(height);
	writeFIFO(fg);
	writeFIFO(bg);
}

static void write_glyph(char c, uint32 x, uint32 y, uint32 fg, uint32 bg)
{
	copy_bitmap(1, 0, vcons.glyph_height * (int)c, x, y, vcons.glyph_width, vcons.glyph_height, fg, bg);
}

static void load_font()
{
	vcons.glyph_width = CHAR_WIDTH;
	vcons.glyph_height = CHAR_HEIGHT;
	vcons.text_cols = vcons.scrn_width / vcons.glyph_width;
	vcons.text_rows = vcons.scrn_height / vcons.glyph_height;
	define_font(1, ((CHAR_WIDTH + 7) & ~7), sizeof(FONT), FONT);
	dprintf("load_font: %dx%d glyphs\n", vcons.text_cols, vcons.text_rows);
}

static int set_mode(uint32 width, uint32 height)
{
	out_reg(SVGA_REG_WIDTH, width);
	out_reg(SVGA_REG_HEIGHT, height);
	vcons.fb_offset = in_reg(SVGA_REG_FB_OFFSET);
	vcons.bytes_per_line = in_reg(SVGA_REG_BYTES_PER_LINE);
	vcons.red_mask = in_reg(SVGA_REG_RED_MASK);
	vcons.green_mask = in_reg(SVGA_REG_GREEN_MASK);
	vcons.blue_mask = in_reg(SVGA_REG_BLUE_MASK);
	//dprintf("fb_offset: %u\n", vcons.fb_offset);
	//dprintf("bytes/line: %u\n", vcons.bytes_per_line);
	//dprintf("rgb masks: %x:%x:%x\n", vcons.red_mask, vcons.green_mask, vcons.blue_mask);
	out_reg(SVGA_REG_ENABLE, 1);
	vcons.scrn_width = width;
	vcons.scrn_height = height;
	return 0;
}

int find_and_map(void)
{
	static uint32 vendor_ids[] = { 1, PCI_VENDOR_ID_VMWARE };
	static uint32 device_ids[] = { 1, PCI_DEVICE_ID_VMWARE_SVGA2 };
	device dev;
	aspace_id kai = vm_get_kernel_aspace_id();
	int err = bus_find_device(1, (id_list*)vendor_ids, (id_list*)device_ids, &dev);
	if (err < 0) goto error0;

	dprintf("vmware SVGA2 device detected at '%s'\n", dev.dev_path);
	vcons.ports = dev.base[0];
	vcons.fb_phys_base = dev.base[1];
	vcons.fb_size = MIN(dev.size[1], SVGA_FB_MAX_SIZE);
	vcons.fifo_phys_base = dev.base[2];
	vcons.fifo_size = MIN(dev.size[2], SVGA_MEM_SIZE);
	vcons.fb_region = vm_map_physical_memory(kai, "vmw:fb", (void **)&vcons.fb_base, REGION_ADDR_ANY_ADDRESS, vcons.fb_size, LOCK_KERNEL | LOCK_RW, vcons.fb_phys_base);
	if (vcons.fb_region < 0)
	{
		err = vcons.fb_region;
		dprintf("Error mapping frame buffer: %x\n", err);
		goto error0;
	}
	//dprintf("vmw:fb mapped from %x to %x\n", (uint32)vcons.fb_phys_base, (uint32)vcons.fb_base);
	vcons.fifo_region = vm_map_physical_memory(kai, "vmw:fifo", (void **)&vcons.fifo_base, REGION_ADDR_ANY_ADDRESS, vcons.fifo_size, LOCK_KERNEL | LOCK_RW, vcons.fifo_phys_base);
	if (vcons.fifo_region < 0)
	{
		err = vcons.fifo_region;
		dprintf("Error mapping vmw::fifo: %x\n", err);
		goto error1;
	}
	//dprintf("vmw:fifo mapped from %x to %x\n", (uint32)vcons.fifo_phys_base, (uint32)vcons.fifo_base);
	out_reg(SVGA_REG_ID, SVGA_ID_2);
	//dprintf("SVGA_REG_ID: %x\n", in_reg(SVGA_REG_ID));
#if 0
	{
		uint32 fb_start = in_reg(SVGA_REG_FB_START);
		uint32 mem_start = in_reg(SVGA_REG_MEM_START);
		uint32 max_width = in_reg(SVGA_REG_MAX_WIDTH);
		uint32 max_height = in_reg(SVGA_REG_MAX_HEIGHT);
		dprintf("fb  : %x\nfifo: %x\n", fb_start, mem_start);
		dprintf("w:h:b %lu:%lu:%lu\n", max_width, max_height, bits_per_pixel);
	}
#endif
	vcons.bits_per_pixel = in_reg(SVGA_REG_BITS_PER_PIXEL);
	goto error0;
error2:
	// unmap vcons.fifo_region
	vm_delete_region(kai, vcons.fifo_region);
error1:
	// unmap vcons.fb_region
	vm_delete_region(kai, vcons.fb_region);
error0:
	return err;
}

int console_dev_init(kernel_args *ka)
{
	if(!ka->fb.enabled)
	{
		int err = find_and_map();
		if (err < 0) return err;
		vcons.cursor_visible = 0;
		vcons.fg = 0;
		vcons.bg = 0xffffff;
		setup_bit_reversed();
		init_fifo();
		set_mode(800,600);
		clear_screen();
		dprintf("screen cleared\n");
#if 0
		init_cursor();
		dprintf("cursor initialized\n");
		show_cursor(1);
		dprintf("cursor shown\n");
#endif
		load_font();
		dprintf("font loaded\n");

		gotoxy(0, ka->cons_line);
		// update_cursor(vcons.cursor_x, vcons.cursor_y);
		show_cursor(0);

		mutex_init(&console_lock, "console_lock");
		keyboard_fd = sys_open("/dev/keyboard", STREAM_TYPE_DEVICE, 0);
		if(keyboard_fd < 0)
			panic("console_dev_init: error opening /dev/keyboard\n");

		// create device node
		devfs_publish_device("console", NULL, &console_hooks);
	}

	return 0;
}

