/*
** Copyright 2003, Justin Smith. All rights reserved.
** Distributed under the terms of the NewOS License.
** 2003/09/01
*/
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 64

/* gross hack to get this to link inside the kernel */
#if KERNEL
#include <kernel/vfs.h>
#define read(fd, buf, size) sys_read(fd, buf, -1, size)
#else
#include <sys/syscalls.h>
#endif

int _v_scanf( int (*_read)(void*), void (*_push)(void*, unsigned char),  void* arg, const unsigned char *, va_list);

/**********************************************************/
/* sscanf functions                                       */
/**********************************************************/

struct _sscanf_struct
{

    int count;
	int buff_pos;
	unsigned char* buff;
	unsigned char const *str;
    int pos;

};

static void _sscanf_push(void *p, unsigned char c)
{
	struct _sscanf_struct *val = (struct _sscanf_struct*)p;
	if(val->buff_pos < MAX_BUFF_SIZE)
		val->buff[val->buff_pos++] = c;

}

static int _sscanf_read(void* p)
{
	struct _sscanf_struct *val = (struct _sscanf_struct*)p;
    int ch;
	if(val->buff_pos > 0)
	{
		return val->buff[--val->buff_pos];
	}

    if((ch = val->str[val->pos++]) == 0)
    {
        val->pos--;
        return 0;
    }
    val->count++;
	return ch;
}

int vsscanf(char const *str, char const *format, va_list arg_ptr)
{
    struct _sscanf_struct *val = (struct _sscanf_struct*)malloc(sizeof(struct _sscanf_struct));

	val->buff = (unsigned char*)malloc(MAX_BUFF_SIZE*sizeof(unsigned char));
	val->buff_pos = 0;
	val->count = 0;
    val->pos = 0;
    val->str = str;
    return _v_scanf(_sscanf_read, _sscanf_push, &val, format, arg_ptr);
}

/**********************************************************/

/* fscanf functions                                       */

/**********************************************************/

struct _fscanf_struct
{
	int buff_pos;
	unsigned char* buff;
	int count;
	FILE* stream;
};

static void _fscanf_push(void *p, unsigned char c)
{
	struct _fscanf_struct *val = (struct _fscanf_struct*)p;
	if(val->buff_pos < MAX_BUFF_SIZE)
		val->buff[val->buff_pos++] = c;
}

static int _fscanf_read(void *p)
{
	struct _fscanf_struct *val = (struct _fscanf_struct*)p;
	unsigned char c;
	FILE* stream;

	if(val->buff_pos > 0)
	{
		return val->buff[--val->buff_pos];
	}

	stream = val->stream;

    if (stream->rpos >= stream->buf_pos)
    {
        int len = read(stream->fd, stream->buf, stream->buf_size);
        if (len==0)
        {
            stream->flags |= _STDIO_EOF;
            return EOF;
        }
        else if (len < 0)
        {
            stream->flags |= _STDIO_ERROR;
            return EOF;
        }
        stream->rpos=0;
        stream->buf_pos=len;
    }
	c = stream->buf[stream->rpos++];
/*
	write(1, "read: \'", 7);
	write(1, &c, 1);
	write(1, "\'\r\n", 3);
*/
	val->count++;
    return (int)c;
}

int vfscanf( FILE *stream, char const *format, va_list arg_ptr)
{
	struct _fscanf_struct *val = (struct _fscanf_struct*)malloc(sizeof(struct _fscanf_struct));

	val->buff = (unsigned char*)malloc(MAX_BUFF_SIZE*sizeof(unsigned char));
	val->buff_pos = 0;
	val->count = 0;
	val->stream = stream;
    return _v_scanf(_fscanf_read, _fscanf_push, val, format, arg_ptr);
}

/**********************************************************/
/* value size determination functions                     */
/**********************************************************/

typedef enum
{
	SHORT,
	INT,
	LONG,
	LONGLONG
} valSize;
/*
static valSize _getValSize(char c, int (*_read)(void*), void* arg)
{
	if(c == 'h')
	{
		return SHORT;
	}
	if(c == 'l')
	{
		return LONG;
	}
	if( c == 'L')
	{
		return LONGLONG;
	}
	return INT;
}
*/
/**********************************************************/
/* utility functions                                      */
/**********************************************************/
/*
static bool _isIntegralType(char c)
{
	return c == 'd' || c == 'i' || c == 'o' || c == 'u' || c == 'x'
		|| c == 'X' || c == 'p';

}

static bool _isFloatingType(char c)
{
	return c == 'E' || c == 'G'
		|| c == 'e' || c == 'f' || c == 'g';
}

static bool _isType(char c)
{
	return _isIntegralType(c) || _isFloatingType(c) || c == 'n' || c == 'c' || c == '%' || c == '[';
}
*/

static int _read_next_non_space( int (*_read)(void*), void* arg)
{
	int c = _read(arg);
	while(isspace(c))
	{
		c = _read(arg);
	}
	return c;
}

/*  The code for this function is mostly taken from strtoull */
static unsigned long long convertIntegralValue(int (*_read)(void*), void (*_push)(void*, unsigned char), void *arg, int base, short width)
{
	unsigned long long acc;
	int c;
	unsigned long long qbase, cutoff;
	int neg, any, cutlim;
	int length = 0;


	do {
		c = _read(arg);
	} while (isspace(c));

	if(c <= 0)
	{
		// what to do?
		return 0;
	}

	if(width == 0)
	{
		return 0;
	}

	if (c == (int)'-') {
		neg = 1;
		length++;
		c = _read(arg);
	} else {
		neg = 0;
		if (c == (int)'+')
		{
			length++;
			c = _read(arg);
		}
	}

	if(c <= 0)
	{
		// what to do?
		return 0;
	}

	if(width > 0 && length >= width)
	{
		return 0;
	}

	if (base == 0 || base == 16)
	{
		unsigned char c_next = _read(arg);
	    if(c == '0' && (c_next == 'x' || c_next == 'X'))
		{
			c = _read(arg);
			base = 16;
		}
		else
		{
			if(c <= 0 || c_next <= 0)
			{
				// what to do?
				return 0;
			}
			_push(arg, c_next);
		}
	}

	if(c <= 0)
	{
		// what to do?
		return 0;
	}

	if(width > 0 && length >= width)
	{
		return 0;
	}

	if (base == 0)
	{
		base = c == '0' ? 8 : 10;
	}

	qbase = (unsigned)base;
	cutoff = (unsigned long long)ULLONG_MAX / qbase;
	cutlim = (unsigned long long)ULLONG_MAX % qbase;


	for (acc = 0, any = 0;; c = _read(arg), length++)
	{
		if(c <= 0)
		{
			goto finish_EOF;
		}
		if(width > 0 && length > width)
		{
			goto finish;
		}

		if (!isascii(c))
		break;
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else
		{
			any = 1;
			acc *= qbase;

			acc += c;
		}
	}

finish:
	_push(arg, c);

finish_EOF:
	if (any < 0)
	{
		acc = ULLONG_MAX;
	}
	else if (neg)
		acc = -acc;

	return acc;
}


/**********************************************************/
/* the function                                           */
/**********************************************************/
int _v_scanf( int (*_read)(void*), void (*_push)(void*, unsigned char),  void* arg, const unsigned char *format, va_list arg_ptr)
{
	unsigned int fieldsRead = 0;
	unsigned char fch;

//	write(1, "\r\n_v_scanf\r\n", 12);

	while (*format)
	{
		long long s_temp = 0;
		fch = *format++;
/*
		write(1, "fch: \'", 6);
		write(1, &fch, 1);
		write(1, "\'\r\n", 3);
*/

    	if(isspace(fch))
    	{
		 	if(isspace(_read(arg)))
			{
				break;
			}
			return EOF;
		}
		else if(fch != '%')
		{
			if(fch == _read_next_non_space(_read, arg))
			{
				break;
			}
			return EOF;
		}
		else
		{
			bool suppressAssignment = false;
			short width = -1;
			valSize size = INT;
keeplooking:

			fch = *format++;
/*
			write(1, "fch: \'", 6);
			write(1, &fch, 1);
			write(1, "\'\r\n", 3);
*/
			switch(fch)
			{
				case '%':
					if(_read_next_non_space(_read, arg) != '%')
					{
						return EOF;
					}
					break;
				case '*':
					suppressAssignment = true;
					goto keeplooking;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					if(width == -1)
					{
						width = 0;
					}
					do
					{
						width = 10 * width + (unsigned int)(fch - (unsigned char)'0');
						fch = *format++;
					}
					while(isdigit(fch));
					format--;
					goto keeplooking;

				case 'h':
					size = SHORT;
					goto keeplooking;
				case 'l':
					size = LONG;
					goto keeplooking;
				case 'L':
					size = LONGLONG;
					goto keeplooking;
				case 'u':
				{
					unsigned long long u_temp = convertIntegralValue(_read, _push, arg, 10, width);
					fieldsRead++;

					switch(size)
					{
						case SHORT:
						{
							unsigned short* us = (unsigned short*)va_arg(arg_ptr, unsigned short*);
							if(u_temp > USHRT_MAX)
							{
								*us = USHRT_MAX;
								break;
							}
							*us = (unsigned short)u_temp;
						}
						break;
						case INT:
						{
							unsigned int *ui = (unsigned int*)va_arg(arg_ptr, unsigned int*);
							if(u_temp > UINT_MAX)
							{
								*ui = UINT_MAX;
								break;
							}
							*ui = (unsigned int)u_temp;
						}
						break;
						case LONG:
						{
							unsigned long *ul = (unsigned long*)va_arg(arg_ptr, unsigned long*);
							if(u_temp > ULONG_MAX)
							{
								*ul = ULONG_MAX;
								break;
							}
							*ul = (unsigned long)u_temp;
						}
						break;
						default:
						{
							unsigned long long *ull = (unsigned long long*)va_arg(arg_ptr, unsigned long long*);
							*ull = (unsigned long long)u_temp;
						}
						break;
					}
				}
				break;
				case 'i':
					s_temp = (long long)convertIntegralValue(_read, _push, arg, 0, width);
					goto read_signed_int;
				case 'd':
					s_temp = (long long)convertIntegralValue(_read, _push, arg, 10, width);
					goto read_signed_int;
				case 'o':
					s_temp = (long long)convertIntegralValue(_read, _push, arg, 8, width);
					goto read_signed_int;
				case 'x':
				case 'X':
				case 'p':
				{
					s_temp = (long long)convertIntegralValue(_read, _push, arg, 16, width);
read_signed_int:
					fieldsRead++;
					switch(size)
					{
						case SHORT:
						{
							short* s = (short*)va_arg(arg_ptr, short*);
							if(s_temp > SHRT_MAX)
							{
								*s = SHRT_MAX;
								break;
							}
							else if(s_temp < SHRT_MIN)
							{
								*s = SHRT_MIN;
								break;
							}
							*s = (unsigned short)s_temp;
						}
						break;
						case INT:
						{
							int *i = (int*)va_arg(arg_ptr, int*);
							if(s_temp > INT_MAX)
							{
								*i = INT_MAX;
								break;
							}
							else if(s_temp < INT_MIN)
							{
								*i = INT_MIN;
							}
							*i = (unsigned int)s_temp;
						}
						break;
						case LONG:
						{
							long *l = (long*)va_arg(arg_ptr, long*);
							if(s_temp > LONG_MAX)
							{
								*l = LONG_MAX;
								break;
							}
							else if(s_temp < LONG_MIN)
							{
								*l = LONG_MIN;
							}
							*l = (unsigned long)s_temp;
						}
						break;
						default:
						{
							long long *ll = (long long*)va_arg(arg_ptr, unsigned long long*);
							*ll = (long long)s_temp;
						}
					}
				}
				break;
				case 'c':
				{
					width = 10;
//					write(1, "hello", 5);
					unsigned char* c = (unsigned char*)va_arg(arg_ptr, unsigned char*);
					int i = 0;
					if(width == -1)
						width = 1;
					for(; i < width; i++)
					{
						int temp = _read(arg);
						if(temp < 0)
						{
							break;
						}
						*(c+i) = temp;
						if(!temp)
							break;
					}
				}
				break;
				// Dangerous to use without specifying width.
				case 's':
				{
					width = 10;
					unsigned char* c = (unsigned char*)va_arg(arg_ptr, unsigned char*);
					int i = 0;
					do
					{
						int temp = _read(arg);
						if(temp <= 0 || isspace(temp))
							break;
						*(c+i++) = temp;
					}while(!(width >= 0 && i >= width));
					*(c+i) = '\0';
					break;
				}
				default:
					// not yet implemented
					return EOF;
			}
		}
	}
	return fieldsRead;
}

