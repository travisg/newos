/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/mod_console.h>
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <newos/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <string.h>
#include <stdio.h>

#include <kernel/dev/console/console.h>

#define TAB_SIZE 8
#define TAB_MASK 7

typedef enum {
	CONSOLE_STATE_NORMAL = 0,
	CONSOLE_STATE_GOT_ESCAPE,
	CONSOLE_STATE_SEEN_BRACKET,
	CONSOLE_STATE_NEW_ARG,
	CONSOLE_STATE_PARSING_ARG,
} console_state;

#define MAX_ARGS 8

static struct console_desc {
	mutex lock;

	int lines;
	int columns;

	unsigned char attr;
	unsigned char saved_attr;
	bool          bright_attr;
	bool          reverse_attr;

	int x;						/* current x coordinate */
	int y;						/* current y coordinate */
	int saved_x;				/* used to save x and y */
	int saved_y;

	int scroll_top;	/* top of the scroll region */
	int scroll_bottom;	/* bottom of the scroll region */

	/* state machine */
	console_state state;
	int arg_ptr;
	int args[MAX_ARGS];
	mod_console *funcs;
} gconsole;

#define update_cursor(con, x, y) (con->funcs->move_cursor)(x, y)

static void gotoxy(struct console_desc *con, int new_x, int new_y)
{
	if(new_x >= con->columns)
		new_x = con->columns - 1;
	if(new_x < 0)
		new_x = 0;
	if(new_y >= con->lines)
		new_y = con->lines - 1;
	if(new_y < 0)
		new_y = 0;

	con->x = new_x;
	con->y = new_y;
}

static void reset_console(struct console_desc *con)
{
	con->attr = 0x0f;
	con->scroll_top = 0;
	con->scroll_bottom = con->lines - 1;
	con->bright_attr = true;
	con->reverse_attr = false;
}

/* scroll from the cursor line up to the top of the scroll region up one line */
static void scrup(struct console_desc *con)
{
	// see if cursor is outside of scroll region
	if((con->y < con->scroll_top) || (con->y > con->scroll_bottom))
		return;

	if(con->y - con->scroll_top > 1) {
		// move the screen up one
		con->funcs->blt(0, con->scroll_top + 1, con->columns, con->y - con->scroll_top, 0, con->scroll_top);
	}

	// clear the bottom line
	con->funcs->fill_glyph(0, con->y, con->columns, 1, ' ', con->attr);
}

/* scroll from the cursor line down to the bottom of the scroll region down one line */
static void scrdown(struct console_desc *con)
{
	// see if cursor is outside of scroll region
	if((con->y < con->scroll_top) || (con->y > con->scroll_bottom))
		return;

	if(con->scroll_bottom - con->y > 1) {
		// move the screen down one
		con->funcs->blt(0, con->y, con->columns, con->scroll_bottom - con->y, 0, con->y + 1);
	}

	// clear the top line
	con->funcs->fill_glyph(0, con->y, con->columns, 1, ' ', con->attr);
}

static void lf(struct console_desc *con)
{
//	dprintf("lf: y %d x %d scroll_top %d scoll_bottom %d\n", con->y, con->x, con->scroll_top, con->scroll_bottom);
	if(con->y == con->scroll_bottom ) {
 		// we hit the bottom of our scroll region
 		scrup(con);
	} else if(con->y < con->scroll_bottom) {
		con->y++;
	}
}

static void rlf(struct console_desc *con)
{
	if(con->y == con->scroll_top) {
 		// we hit the top of our scroll region
 		scrdown(con);
	} else if(con->y > con->scroll_top) {
		con->y--;
	}
}

static void cr(struct console_desc *con)
{
	con->x = 0;
}

static void del(struct console_desc *con)
{
	if (con->x > 0) {
		con->x--;
	} else if(con->y > 0) {
        con->y--;
        con->x = con->columns - 1;
    } else {
        //This doesn't work...
        //scrdown(con);
        //con->y--;
        //con->x = con->columns - 1;
        return;
    }
	con->funcs->put_glyph(con->x, con->y, ' ', con->attr);
}

typedef enum {
	LINE_ERASE_WHOLE,
	LINE_ERASE_LEFT,
	LINE_ERASE_RIGHT
} erase_line_mode;

static void erase_line(struct console_desc *con, erase_line_mode mode)
{
	switch(mode) {
		case LINE_ERASE_WHOLE:
			con->funcs->fill_glyph(0, con->y, con->columns, 1, ' ', con->attr);
			break;
		case LINE_ERASE_LEFT:
			con->funcs->fill_glyph(0, con->y, con->x+1, 1, ' ', con->attr);
			break;
		case LINE_ERASE_RIGHT:
			con->funcs->fill_glyph(con->x, con->y, con->columns - con->x, 1, ' ', con->attr);
			break;
		default:
			return;
	}
}

typedef enum {
	SCREEN_ERASE_WHOLE,
	SCREEN_ERASE_UP,
	SCREEN_ERASE_DOWN
} erase_screen_mode;

static void erase_screen(struct console_desc *con, erase_screen_mode mode)
{
	switch(mode) {
		case SCREEN_ERASE_WHOLE:
			con->funcs->fill_glyph(0, 0, con->columns, con->lines, ' ', con->attr);
			break;
		case SCREEN_ERASE_UP:
			con->funcs->fill_glyph(0, 0, con->columns, con->y + 1, ' ', con->attr);
			break;
		case SCREEN_ERASE_DOWN:
			con->funcs->fill_glyph(con->y, 0, con->columns, con->lines - con->y, ' ', con->attr);
			break;
		default:
			return;
	}
}

static void save_cur(struct console_desc *con, bool save_attrs)
{
	con->saved_x = con->x;
	con->saved_y = con->y;
	if(save_attrs)
		con->saved_attr = con->attr;
}

static void restore_cur(struct console_desc *con, bool restore_attrs)
{
	con->x = con->saved_x;
	con->y = con->saved_y;
	if(restore_attrs)
		con->attr = con->saved_attr;
}

static char console_putch(struct console_desc *con, const char c)
{
	if(++con->x >= con->columns) {
		cr(con);
		lf(con);
	}
	con->funcs->put_glyph(con->x-1, con->y, c, con->attr);
	return c;
}

static void tab(struct console_desc *con)
{
	con->x = (con->x + TAB_SIZE) & ~TAB_MASK;
	if (con->x >= con->columns) {
		con->x -= con->columns;
		lf(con);
	}
}

static void set_scroll_region(struct console_desc *con, int top, int bottom)
{
	if(top < 0)
		top = 0;
	if(bottom >= con->lines)
		bottom = con->lines - 1;
	if(top > bottom)
		return;

	con->scroll_top = top;
	con->scroll_bottom = bottom;
}

static int console_open(dev_ident ident, dev_cookie *cookie)
{
	struct console_desc *con = (struct console_desc *)ident;

	*cookie = (dev_cookie)con;

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
	return ERR_NOT_ALLOWED;
}

static void set_vt100_attributes(struct console_desc *con, int *args, int arg_ptr)
{
	int i;

#define FMASK 0x0f
#define BMASK 0x70

	for(i = 0; i <= arg_ptr; i++) {
		switch(args[i]) {
			case 0: // reset
				con->attr = 0x0f;
				con->bright_attr = true;
				con->reverse_attr = false;
				break;
			case 1: // bright
				con->bright_attr = true;
				con->attr |= 0x08; // set the bright bit
				break;
			case 2: // dim
				con->bright_attr = false;
				con->attr &= ~0x08; // unset the bright bit
				break;
			case 4: // underscore we can't do
				break;
			case 5: // blink
				con->attr |= 0x80; // set the blink bit
				break;
			case 7: // reverse
				con->reverse_attr = true;
				con->attr = ((con->attr & BMASK) >> 4) | ((con->attr & FMASK) << 4);
				if(con->bright_attr)
					con->attr |= 0x08;
				break;
			case 8: // hidden?
				break;

			/* foreground colors */
			case 30: con->attr = (con->attr & ~FMASK) | 0 | (con->bright_attr ? 0x08 : 0); break; // black
			case 31: con->attr = (con->attr & ~FMASK) | 4 | (con->bright_attr ? 0x08 : 0); break; // red
			case 32: con->attr = (con->attr & ~FMASK) | 2 | (con->bright_attr ? 0x08 : 0); break; // green
			case 33: con->attr = (con->attr & ~FMASK) | 6 | (con->bright_attr ? 0x08 : 0); break; // yellow
			case 34: con->attr = (con->attr & ~FMASK) | 1 | (con->bright_attr ? 0x08 : 0); break; // blue
			case 35: con->attr = (con->attr & ~FMASK) | 5 | (con->bright_attr ? 0x08 : 0); break; // magenta
			case 36: con->attr = (con->attr & ~FMASK) | 3 | (con->bright_attr ? 0x08 : 0); break; // cyan
			case 37: con->attr = (con->attr & ~FMASK) | 7 | (con->bright_attr ? 0x08 : 0); break; // white

			/* background colors */
			case 40: con->attr = (con->attr & ~BMASK) | (0 << 4); break; // black
			case 41: con->attr = (con->attr & ~BMASK) | (4 << 4); break; // red
			case 42: con->attr = (con->attr & ~BMASK) | (2 << 4); break; // green
			case 43: con->attr = (con->attr & ~BMASK) | (6 << 4); break; // yellow
			case 44: con->attr = (con->attr & ~BMASK) | (1 << 4); break; // blue
			case 45: con->attr = (con->attr & ~BMASK) | (5 << 4); break; // magenta
			case 46: con->attr = (con->attr & ~BMASK) | (3 << 4); break; // cyan
			case 47: con->attr = (con->attr & ~BMASK) | (7 << 4); break; // white
		}
	}
}

static bool process_vt100_command(struct console_desc *con, const char c, bool seen_bracket, int *args, int num_args)
{
	bool ret = true;

//	dprintf("process_vt100_command: c '%c', num_args %d, arg[0] %d, arg[1] %d, seen_bracket %d\n",
//		c, num_args, args[0], args[1], seen_bracket);

	if(seen_bracket) {
		switch(c) {
			case 'H': /* set cursor position */
			case 'f': {
				int row = (num_args > 0) ? args[0] : 1;
				int col = (num_args > 1) ? args[1] : 1;
				if(row > 0) row--;
				if(col > 0) col--;
				gotoxy(con, col, row);
				break;
			}
			case 'A': { /* move up */
				int deltay = (num_args > 0) ? -args[0] : -1;
				if(deltay == 0) deltay = -1;
				gotoxy(con, con->x, con->y + deltay);
				break;
			}
			case 'e':
			case 'B': { /* move down */
				int deltay = (num_args > 0) ? args[0] : 1;
				if(deltay == 0) deltay = 1;
				gotoxy(con, con->x, con->y + deltay);
				break;
			}
			case 'D': { /* move left */
				int deltax = (num_args > 0) ? -args[0] : -1;
				if(deltax == 0) deltax = -1;
				gotoxy(con, con->x + deltax, con->y);
				break;
			}
			case 'a':
			case 'C': { /* move right */
				int deltax = (num_args > 0) ? args[0] : 1;
				if(deltax == 0) deltax = 1;
				gotoxy(con, con->x + deltax, con->y);
				break;
			}
			case '`':
			case 'G': { /* set X position */
				int newx = (num_args > 0) ? args[0] : 1;
				if(newx > 0) newx--;
				gotoxy(con, newx, con->y);
				break;
			}
			case 'd': { /* set y position */
				int newy = (num_args > 0) ? args[0] : 1;
				if(newy > 0) newy--;
				gotoxy(con, con->x, newy);
				break;
			}
			case 's': /* save current cursor */
				save_cur(con, false);
				break;
			case 'u': /* restore cursor */
				restore_cur(con, false);
				break;
			case 'r': { /* set scroll region */
				int low = (num_args > 0) ? args[0] : 1;
				int high = (num_args > 1) ? args[1] : con->lines;
				if(low <= high) set_scroll_region(con, low - 1, high - 1);
				break;
			}
			case 'L': { /* scroll virtual down at cursor */
				int lines = (num_args > 0) ? args[0] : 1;
				while(lines > 0) {
					scrdown(con);
					lines--;
				}
				break;
			}
			case 'M': { /* scroll virtual up at cursor */
				int lines = (num_args > 0) ? args[0] : 1;
				while(lines > 0) {
					scrup(con);
					lines--;
				}
				break;
			}
			case 'K':
				if(num_args < 0) {
					// erase to end of line
					erase_line(con, LINE_ERASE_RIGHT);
				} else {
					if(args[0] == 1)
						erase_line(con, LINE_ERASE_LEFT);
					else if(args[0] == 2)
						erase_line(con, LINE_ERASE_WHOLE);
				}
				break;
			case 'J':
				if(num_args < 0) {
					// erase to end of screen
					erase_screen(con, SCREEN_ERASE_DOWN);
				} else {
					if(args[0] == 1)
						erase_screen(con, SCREEN_ERASE_UP);
					else if(args[0] == 2)
						erase_screen(con, SCREEN_ERASE_WHOLE);
				}
				break;
			case 'm':
				if(num_args >= 0) {
					set_vt100_attributes(con, args, num_args);
				}
				break;
			default:
				ret = false;
		}
	} else {
		switch(c) {
			case 'c':
				reset_console(con);
				break;
			case 'D':
				rlf(con);
				break;
			case 'M':
				lf(con);
				break;
			case '7':
				save_cur(con, true);
				break;
			case '8':
				restore_cur(con, true);
				break;
			default:
				ret = false;
		}
	}

	return ret;
}

// used by kprintf() to draw directly onto the screen
static ssize_t _console_write_raw(struct console_desc *con, const void *buf, ssize_t len)
{
	const char *c;
	ssize_t pos = 0;

	while(pos < len) {
		c = &((const char *)buf)[pos++];
		switch(*c) {
			case '\n':
				cr(con);
				lf(con);
				break;
			case 0x8: // backspace
				del(con);
				break;
			case '\t':
				tab(con);
				break;
			case '\r':
			case '\0':
			case 0x1b:
				break;
			default:
				console_putch(con, *c);
		}
	}
	return pos;
}

// used by kprintf_xy() to draw directly onto the screen. Do not update the console's current x y pos
static ssize_t _console_write_raw_xy(struct console_desc *in_con, const void *buf, ssize_t len, int x, int y)
{
	const char *c;
	ssize_t pos = 0;
	struct console_desc con;

	memcpy(&con, in_con, sizeof(con)); // not great, but it keeps us from updating the system's x y

	gotoxy(&con, x, y);

	while(pos < len) {
		c = &((const char *)buf)[pos++];
		switch(*c) {
			case '\n':
				cr(&con);
				lf(&con);
				break;
			case 0x8: // backspace
				del(&con);
				break;
			case '\t':
				tab(&con);
				break;
			case '\r':
			case '\0':
			case 0x1b:
				break;
			default:
				console_putch(&con, *c);
		}
	}
	return pos;
}

static ssize_t _console_write(struct console_desc *con, const void *buf, size_t len)
{
	const char *c;
	size_t pos = 0;

	while(pos < len) {
		c = &((const char *)buf)[pos++];

		switch(con->state) {
		case CONSOLE_STATE_NORMAL:
			// just output the stuff
			switch(*c) {
				case '\n':
					lf(con);
					break;
				case '\r':
					cr(con);
					break;
				case 0x8: // backspace
					del(con);
					break;
				case '\t':
					tab(con);
					break;
				case '\0':
					break;
				case 0x1b:
					// escape character
					con->arg_ptr = -1;
					con->state = CONSOLE_STATE_GOT_ESCAPE;
					break;
				default:
					console_putch(con, *c);
			}
			break;
		case CONSOLE_STATE_GOT_ESCAPE:
			// look for either commands with no argument, or the '[' character
			switch(*c) {
				case '[':
					con->state = CONSOLE_STATE_SEEN_BRACKET;
					break;
				default:
					process_vt100_command(con, *c, false, con->args, con->arg_ptr + 1);
					con->state = CONSOLE_STATE_NORMAL;
			}
			break;
		case CONSOLE_STATE_SEEN_BRACKET:
			switch(*c) {
				case '0'...'9':
					con->arg_ptr = 0;
					con->args[con->arg_ptr] = *c - '0';
					con->state = CONSOLE_STATE_PARSING_ARG;
					break;
				default:
					process_vt100_command(con, *c, true, con->args, con->arg_ptr + 1);
					con->state = CONSOLE_STATE_NORMAL;
			}
			break;
		case CONSOLE_STATE_NEW_ARG:
			switch(*c) {
				case '0'...'9':
					con->arg_ptr++;
					if(con->arg_ptr == MAX_ARGS) {
						con->state = CONSOLE_STATE_NORMAL;
						break;
					}
					con->args[con->arg_ptr] = *c - '0';
					con->state = CONSOLE_STATE_PARSING_ARG;
					break;
				default:
					process_vt100_command(con, *c, true, con->args, con->arg_ptr + 1);
					con->state = CONSOLE_STATE_NORMAL;
			}
			break;
		case CONSOLE_STATE_PARSING_ARG:
			// parse args
			switch(*c) {
				case '0'...'9':
					con->args[con->arg_ptr] *= 10;
					con->args[con->arg_ptr] += *c - '0';
					break;
				case ';':
					con->state = CONSOLE_STATE_NEW_ARG;
					break;
				default:
					process_vt100_command(con, *c, true, con->args, con->arg_ptr + 1);
					con->state = CONSOLE_STATE_NORMAL;
			}
		}
	}

	return pos;
}

static ssize_t console_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	ssize_t err;
	struct console_desc *con = (struct console_desc *)cookie;

//	dprintf("console_write: entry, len = %d\n", *len);

	mutex_lock(&con->lock);

	update_cursor(con, -1, -1); // hide it
	err = _console_write(con, buf, len);
	update_cursor(con, con->x, con->y);

	mutex_unlock(&con->lock);

	return err;
}

static int console_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	int err;
	struct console_desc *con = (struct console_desc *)cookie;

	switch(op) {
		case CONSOLE_OP_WRITEXY: {
			int x,y;
			mutex_lock(&con->lock);

			x = ((int *)buf)[0];
			y = ((int *)buf)[1];

			save_cur(con, false);
			gotoxy(con, x, y);
			if(_console_write(con, ((char *)buf) + 2*sizeof(int), len - 2*sizeof(int)) > 0)
				err = 0; // we're okay
			else
				err = ERR_IO_ERROR;
			restore_cur(con, false);
			mutex_unlock(&con->lock);
			break;
		}
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static int con_kputs(const char *str)
{
	if(!str)
		return 0;
	
	return _console_write_raw(&gconsole, str, strlen(str));
}

static int con_kputs_xy(const char *str, int x, int y)
{
	if(!str)
		return 0;
	
	return _console_write_raw_xy(&gconsole, str, strlen(str), x, y);
}

static int con_kputchar_xy(char c, int x, int y)
{
	struct console_desc con;

	memcpy(&con, &gconsole, sizeof(con)); // not great, but it keeps us from updating the system's x y
	gotoxy(&con, x, y);
	
	console_putch(&con, c);

	return 1;
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

static kernel_console_ops k_console_ops = {
	&con_kputs,
	&con_kputs_xy,
	&con_kputchar_xy
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
#if 0
	if(!ka->fb.enabled) {
		dprintf("con_init: mapping vid mem\n");
		vm_map_physical_memory(vm_get_kernel_aspace_id(), "vid_mem", (void *)&gconsole.origin, REGION_ADDR_ANY_ADDRESS,
			SCREEN_END - SCREEN_START, LOCK_RW|LOCK_KERNEL, SCREEN_START);
		dprintf("con_init: mapped vid mem to virtual address 0x%x\n", gconsole.origin);

		// set up the console structure
		mutex_init(&gconsole.lock, "console_lock");
		gconsole.attr = 0x0f;
		gconsole.lines = LINES;
		gconsole.columns = COLUMNS;
		gconsole.end = gconsole.origin + gconsole.columns * gconsole.lines * 2;
		gconsole.scroll_top = 0;
		gconsole.scroll_bottom = gconsole.lines - 1;
		gconsole.bright_attr = true;
		gconsole.reverse_attr = false;

		gotoxy(&gconsole, 0, ka->cons_line);
		update_cursor(&gconsole, gconsole.x, gconsole.y);
		save_cur(&gconsole, true);

		// create device node
		devfs_publish_device("console", (dev_ident)&gconsole, &console_hooks);
	}
#endif
	// iterate through the list of console modules until we find one that accepts the job
	modules_cookie mc = module_open_list("console");
	char buffer[SYS_MAX_PATH_LEN];
	char saved[SYS_MAX_PATH_LEN];
	size_t bufsize = sizeof(buffer);
	int found = 0;
	*saved = '\0';
	while (!found && (read_next_module_name(mc, buffer, &bufsize) == 0))
	{
		char *special = buffer;
#if ARCH == ARCH_I386
		dprintf("ARCH_I386: using \"/text\" as the fallback driver\n");
		special = strstr(buffer, "/text");
#else
#endif
		if (special && (special != buffer))
		{
			strlcpy(saved, buffer, sizeof(saved));
			dprintf("con_init: skipping %s as the fallback module\n", saved);
		}
		else
		{
			dprintf("con_init: trying module %s\n", buffer);
			found = (module_get(buffer, 0, (void **)&(gconsole.funcs)) == 0);
		}
		bufsize = sizeof(buffer);
	}
	close_module_list(mc);
	// try the fallback module, if required.
	if (!found && strlen(saved))
		found = (module_get(saved, 0, (void **)&(gconsole.funcs)) == 0);
	if (found)
	{
		// set up the console structure
		mutex_init(&gconsole.lock, "console_lock");
		gconsole.funcs->get_size(&gconsole.columns, &gconsole.lines);

		reset_console(&gconsole);
		gotoxy(&gconsole, 0, global_kernel_args.cons_line);
		update_cursor((&gconsole), gconsole.x, gconsole.y);
		save_cur(&gconsole, true);

		// create device node
		devfs_publish_device("console", (dev_ident)&gconsole, &console_hooks);
	}
	else dprintf("con_init: failed to find a suitable module :-(\n");

	if(found) {
		// register with the kernel console
		con_register_ops(&k_console_ops);
	}

	return found ? 0 : -1;
}

