#include "libc.h"
#include "of.h"
#include "floppy.h"

static int of_input_handle = 0;
static int of_output_handle = 0;
static int of_serial_input_handle = 0;
static int of_serial_output_handle = 0;

void _start(int arg1, int arg2, void *openfirmware)
{
	int chosen;
	
	of_init(openfirmware);

	/* open the input and output handle */
	chosen = of_finddevice("/chosen");
	of_getprop(chosen, "stdin", &of_input_handle, sizeof(of_input_handle));
	of_getprop(chosen, "stdout", &of_output_handle, sizeof(of_output_handle));

	puts("this is a test\n");

	init_floppy();

	of_exit();
}

#if 0
int printf(const char *fmt, ...)
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
#endif

void puts(char *str)
{
	while(*str) {
		if(*str == '\n')
			putchar('\r');
		putchar(*str);
		str++;
	}
}

void putchar(char c)
{
	if(of_output_handle != 0)
		of_write(of_output_handle, &c, 1);

}

void write_hex(unsigned int val)
{
	char buf[16];
	int pos;
	
	buf[15] = 0;
	for(pos = 14; pos >= 2 && val != 0; pos--) {
		int hex = val % 16;

		buf[pos] = hex < 10 ? hex + '0' : (hex - 10) + 'a';
		val /= 16;
	}
	buf[pos--] = 'x';
	buf[pos] = '0';
	puts(&buf[pos]);
}


void *memcpy(void *_to, const void *_from, size_t len)
{
	char *to = _to;
	const char *from = _from;

	while(len > 0) {
		*to++ = *from++;
		len--;
	}

	return _to;
}
