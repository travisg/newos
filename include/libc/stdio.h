/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
** Modified by Justin Smith 2003/06/17. Changed FILE struct.
*/

#ifndef __newos__libc_stdio__hh__
#define __newos__libc_stdio__hh__


#include <newos/types.h>
#include <sys/cdefs.h>
#include <stdarg.h>


#ifdef __cplusplus
extern "C"
{
#endif

#define _STDIO_READ 1
#define _STDIO_WRITE 2
#define _STDIO_EOF 4
#define _STDIO_ERROR 8 

struct __FILE {
	int fd; 		/* system file descriptor */
    off_t rpos;       /* The first unread buffer position */
	off_t buf_pos;		/* first unwritten buffer position */ 
	char* buf;		/* buffer */
	off_t buf_size;	/* buffer size */
    char unget;     /* for  ungetc */
    int flags;      /* for feof and ferror */
    struct __FILE* next; /* for fflush */
    sem_id sid;     /* semaphore */
};
typedef struct __FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF -1

int printf(char const *format, ...) __PRINTFLIKE(1,2);
int fprintf(FILE *stream, char const *format, ...) __PRINTFLIKE(2,3);
int sprintf(char *str, char const *format, ...) __PRINTFLIKE(2,3);
int snprintf(char *str, size_t size, char const *format, ...) __PRINTFLIKE(3,4);
int asprintf(char **ret, char const *format, ...) __PRINTFLIKE(2,3);
int vprintf(char const *format, va_list ap);
int vfprintf(FILE *stream, char const *format, va_list ap);
int vsprintf(char *str, char const *format, va_list ap);
int vsnprintf(char *str, size_t size, char const *format, va_list ap);
int vasprintf(char **ret, char const *format, va_list ap);

FILE *fopen(char const *, char const *);
int   fflush(FILE *);
int   fclose(FILE *);

int   feof(FILE *);
int   ferror(FILE *);
char *fgets(char *, int, FILE *);
void  clearerr(FILE *);
int   fgetc(FILE *);
int   ungetc(int c, FILE *stream);

int scanf(char const *format, ...);
int fscanf(FILE *stream, char const *format, ...);
int sscanf(char const *str, char const *format, ...);
int vscanf(char const *format, va_list ap);
int vsscanf(char const *str, char const *format, va_list ap);
int vfscanf(FILE *stream, char const *format, va_list ap);

int getchar(void);

int _v_printf(int (*_write)(void*, const void *, ssize_t ), void* arg, const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif


#endif
