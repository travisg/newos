#include "of.h"
#include "serial.h"

void puts(char *str);
void putchar(char c);
void write_hex(unsigned int val);

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

	init_serial();

restart:
	puts("waiting for command\n");

	{
		int base_address;
		int entry_point;
		int length;
		int command;
		unsigned char *ptr;
		void (*func)(int, int, void *);

		serial_read_int32(&command);
		if(command != 0x99) {
			puts("bad command, restarting\n");
			goto restart;
		}

		serial_read_int32(&base_address);
		serial_read_int32(&length);
		serial_read_int32(&entry_point);

		puts("read base and length, claiming\n");

		puts("base ");
		write_hex(base_address);
		puts("\nlength ");
		write_hex(length);
		puts("\nentry_point ");
		write_hex(entry_point);
		puts("\n");

		ptr = (void *)base_address;

		of_claim(base_address, length, 0);

		puts("reading data\n");

		serial_read(ptr, length);	

		puts("done reading data, calling function\n");

		func = (void *)entry_point;
		func(arg1, arg2, openfirmware);
	}

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
