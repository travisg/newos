#include "console.h"
#include "debug.h"
#include "vm.h"
#include "int.h"

#include "arch_cpu.h"
#include "arch_int.h"

#include "string.h"

/* used in keyboard.S */
#define TTY_BUF_SIZE 1024

struct tty_queue {
	unsigned long data;
	unsigned long head;
	unsigned long tail;
	char buf[TTY_BUF_SIZE];
};

struct tty_queue tty_buffer = {
	0, // data field
	0, // head
	0, // tail
	"" // buffer
};

int _keyboard_interrupt(void);

struct tty_queue *table_list = &tty_buffer;
unsigned int origin = 0;

#define SCREEN_START 0xb8000
#define SCREEN_END   0xc0000
#define LINES 25
#define COLUMNS 80
#define NPAR 16

#define scr_end (origin+LINES*COLUMNS*2)

//static unsigned long origin=SCREEN_START;
//static unsigned long scr_end=SCREEN_START+LINES*COLUMNS*2;
static unsigned int pos = 0;
static unsigned int x = 0;
static unsigned int y = 0;
//static unsigned long top=0;
static unsigned int bottom=LINES;
static unsigned int lines=LINES;
static unsigned int columns=COLUMNS;
//static unsigned long state=0;
//static unsigned long npar,par[NPAR];
//static unsigned long ques=0;
static unsigned char attr=0x07;
static int vidport = 0;

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
#define RESPONSE "\033[?1;2c"

static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
	if (new_x>=columns || new_y>=lines)
		return;
	x = new_x;
	y = new_y;
	pos = origin+((y*columns+x)<<1);
}

static void scrup(void)
{
	unsigned long i;
	
	// move the screen up one
	memcpy((void *)origin, (void *)origin+2*COLUMNS, 2*(LINES-1)*COLUMNS);
	
	// set the new position to the beginning of the last line
	pos = origin + (LINES-1)*COLUMNS*2;
	
	// clear the bottom line
	for(i = pos; i < scr_end; i += 2) {
		*(unsigned short *)i = 0x0720;
	}
}
static void lf(void)
{
	if (y+1<bottom) {
		y++;
		pos += columns<<1;
		return;
	}
	scrup();
}
static void cr(void)
{
	pos -= x<<1;
	x=0;
}
#if 0
static void scrdown(void)
{
	unsigned long i;
	
	// move the screen down one
	memcpy(origin+2*COLUMNS, origin, 2*(LINES-1)*COLUMNS);
	
	// set the new position to the beginning of the first line
	pos = origin;
	
	// clear the top line
	for(i = pos; i < pos + 2*COLUMNS; i += 2) {
		*(unsigned short *)i = 0x0720;
	}
}
static void ri(void)
{
	if (y>top) {
		y--;
		pos -= columns<<1;
		return;
	}
	scrdown();
}
static void del(void)
{
	if (x) {
		pos -= 2;
		x--;
		*(unsigned short *)pos = 0x0720;
	}
}

static void csi_J(int par)
{
	long count __asm__("cx");
	long start __asm__("di");

	switch (par) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = (pos-origin)>>1;
			start = origin;
			break;
		case 2: /* erase whole display */
			count = columns*lines;
			start = origin;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (0x0720)
//		:"cx","di");
		);
}

static void csi_K(int par)
{
	long count __asm__("cx");
	long start __asm__("di");

	switch (par) {
		case 0:	/* erase from cursor to end of line */
			if (x>=columns)
				return;
			count = columns-x;
			start = pos;
			break;
		case 1:	/* erase from start of line to cursor */
			start = pos - (x<<1);
			count = (x<columns)?x:columns;
			break;
		case 2: /* erase whole line */
			start = pos - (x<<1);
			count = columns;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (0x0720)
//		:"cx","di");
		);
}

void csi_m(void)
{
	int i;

	for (i=0;i<=npar;i++)
		switch (par[i]) {
			case 0:attr=0x07;break;
			case 1:attr=0x0f;break;
			case 4:attr=0x0f;break;
			case 7:attr=0x70;break;
			case 27:attr=0x07;break;
		}
}
/*
static void set_cursor(void)
{
	cli();
	outb_p(14,0x3d4);
	outb_p(0xff&((pos-SCREEN_START)>>9),0x3d5);
	outb_p(15,0x3d4);
	outb_p(0xff&((pos-SCREEN_START)>>1),0x3d5);
	sti();
}
*/
static void insert_char(void)
{
	int i=x;
	unsigned short tmp,old=0x0720;
	unsigned short * p = (unsigned short *) pos;

	while (i++<columns) {
		tmp=*p;
		*p=old;
		old=tmp;
		p++;
	}
}

static void insert_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom=lines;
	scrdown();
	top=oldtop;
	bottom=oldbottom;
}

static void delete_char(void)
{
	int i;
	unsigned short * p = (unsigned short *) pos;

	if (x>=columns)
		return;
	i = x;
	while (++i < columns) {
		*p = *(p+1);
		p++;
	}
	*p=0x0720;
}

static void delete_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom=lines;
	scrup();
	top=oldtop;
	bottom=oldbottom;
}

static void csi_at(int nr)
{
	if (nr>columns)
		nr=columns;
	else if (!nr)
		nr=1;
	while (nr--)
		insert_char();
}

static void csi_L(int nr)
{
	if (nr>lines)
		nr=lines;
	else if (!nr)
		nr=1;
	while (nr--)
		insert_line();
}

static void csi_P(int nr)
{
	if (nr>columns)
		nr=columns;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_char();
}

static void csi_M(int nr)
{
	if (nr>lines)
		nr=lines;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_line();
}
#endif
static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
	saved_x=x;
	saved_y=y;
}

static void restore_cur(void)
{
	x=saved_x;
	y=saved_y;
	pos=origin+((y*columns+x)<<1);
}

char arch_con_putch(char c)
{
	if(++x>=COLUMNS) {
		cr();
		lf();
	}
	
	*(char *)pos = c;
	*(char *)(pos+1) = attr;

	pos += 2; 

	return c;
}

void arch_con_puts(const char *s)
{
	char c;
	while ( ( c = *s++ ) != '\0' ) {
		if ( c == '\n' ) {
			cr();
			lf();
		} else {
			arch_con_putch(c); 
		}
	}
/*	outb_p(14, vidport);
	outb_p(0xff & (pos >> 9), vidport+1);
	outb_p(15, vidport);
	outb_p(0xff & (pos >> 1), vidport+1);*/
//	set_cursor();
}

void arch_con_puts_xy(const char *s, int x, int y)
{
	save_cur();
	gotoxy(x, y);
	arch_con_puts(s);
	restore_cur();
}

/*
 *  void arch_con_init(void);
 */
int arch_con_init(struct kernel_args *ka)
{
	vidport = 0x3b4;

	dprintf("arch_con_init: mapping vid mem\n");
	vm_map_physical_memory(vm_get_kernel_aspace(), "vid_mem", (void *)&origin, AREA_ANY_ADDRESS,
		SCREEN_END - SCREEN_START, 0, SCREEN_START);
	dprintf("arch_con_init: mapped vid mem to virtual address 0x%x\n", origin);

	pos = origin;

	gotoxy(0, ka->cons_line);

	// Setup keyboard interrupt
	set_intr_gate(0x21,&_keyboard_interrupt);
/*
	outb_p(inb_p(0x21)&0xfd,0x21);
	a=inb_p(0x61);
	outb_p(a|0x80,0x61);
	outb(a,0x61);
*/
	return 0;
}

int handle_keyboard_interrupt()
{
	dprintf("keyboard interrupt\n");
	return 0;
}

