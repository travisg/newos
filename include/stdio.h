/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
** Modified by Justin Smith 2003/06/17. Changed FILE struct.
*/

#ifndef __newos__libc_stdio__hh__
#define __newos__libc_stdio__hh__

#include <stddef.h>
#include <sys/types.h>
#include <sys/cdefs.h>

#include <stdarg.h>

#ifdef __cplusplus
namespace std
{extern "C" {
#endif

#define _STDIO_READ 0x0001
#define _STDIO_WRITE 0x0002
#define _STDIO_EOF 0x0004
#define _STDIO_ERROR 0x0008
#define _STDIO_UNGET 0x0010

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef off_t fpos_t;

struct __FILE {
	int fd; 		/* system file descriptor */
    ptrdiff_t rpos;       /* The first unread buffer position */
	ptrdiff_t buf_pos;		/* first unwritten buffer position */ 
	unsigned char* buf;		/* buffer */
	ptrdiff_t buf_size;	/* buffer size */
    unsigned char unget;     /* for  ungetc */
    int flags;      /* for feof and ferror */
    struct __FILE* next; /* for fflush */
    sem_id sid;     /* semaphore */
};
typedef struct __FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* arguments to setvbuf */
#define _IOFBF  0               /* setvbuf should set fully buffered */
#define _IOLBF  1               /* setvbuf should set line buffered */
#define _IONBF  2               /* setvbuf should set unbuffered */

#define BUFSIZ 1024

#define EOF -1

int printf(char const *format, ...) __PRINTFLIKE(1,2);
int fprintf(FILE *stream, char const *format, ...) __PRINTFLIKE(2,3);
int sprintf(char *str, char const *format, ...) __PRINTFLIKE(2,3);
//int snprintf(char *str, size_t size, char const *format, ...) __PRINTFLIKE(3,4);
//int asprintf(char **ret, char const *format, ...) __PRINTFLIKE(2,3);
int vprintf(char const *format, va_list ap);
int vfprintf(FILE *stream, char const *format, va_list ap);
int vsprintf(char *str, char const *format, va_list ap);
//int vsnprintf(char *str, size_t size, char const *format, va_list ap);
//int vasprintf(char **ret, char const *format, va_list ap);

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int   putc(int ch, FILE *stream);
int   fputc(int ch, FILE *stream);
int   putchar(int c);

int   remove(const char *path);
int   rename(const char *from, const char *to);
void  rewind(FILE *stream);

FILE *tmpfile(void);
char *tmpnam(char *str);
char *tempnam(const char *tmpdir, const char *prefix);

FILE *fopen(char const *, char const *);
int   fflush(FILE *);
int   fclose(FILE *);
FILE *fdopen(int fildes, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);

long int ftell(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, const fpos_t *pos);
int feof(FILE *);
int fseek(FILE *stream, long int offset, int whence);
int fileno(FILE *stream);

int   ferror(FILE *);
void  clearerr(FILE *);
void  perror(const char *string);

int    fputs(const char *str, FILE *stream);
int    puts(const char *str);

FILE *freopen(const char *filename, const char *mode, FILE *stream);
char*  fgets(char *, int, FILE *);
char  *gets(char *str);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int    getchar(void);
int    getc(FILE*);
int    fgetc(FILE *);
int    ungetc(int c, FILE *stream);

void setbuf(FILE *stream, char *buf);
int  setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuffer(FILE *stream, char *buf, int size);
int  setlinebuf(FILE *stream);

int scanf(char const *format, ...);
int fscanf(FILE *stream, char const *format, ...);
int sscanf(char const *str, char const *format, ...);
//int vscanf(char const *format, va_list ap);
int vsscanf(char const *str, char const *format, va_list ap);
int vfscanf(FILE *stream, char const *format, va_list ap);

int _v_printf(int (*_write)(void*, const void *, ssize_t ), void* arg, const char *fmt, va_list args);

#ifdef __cplusplus
}}
#endif

// getchar goes off in it's own little non-standard world
// This function will be removed soon
# ifdef __cplusplus
extern "C"
# endif
int getchar(void);



#endif

#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)
using ::std::FILE;
using ::std::fpos_t;

using ::std::stdin;
using ::std::stdout;
using ::std::stderr;

using ::std::clearerr;
using ::std::fclose;
using ::std::feof;
using ::std::ferror;
using ::std::fflush;
using ::std::fgetc;
using ::std::fgetpos;
using ::std::fgets;
using ::std::fopen;
using ::std::fdopen;
using ::std::freopen;
using ::std::fprintf;
using ::std::fputc;
using ::std::fputs;
using ::std::fread;
using ::std::freopen;
using ::std::fscanf;
using ::std::fseek;
using ::std::fileno;
using ::std::fsetpos;
using ::std::ftell;
using ::std::fwrite;
using ::std::getc;
using ::std::getchar;
using ::std::gets;
using ::std::perror;
using ::std::printf;
using ::std::putc;
using ::std::putchar;
using ::std::puts;
using ::std::remove;
using ::std::rename;
using ::std::rewind;
using ::std::scanf;
using ::std::setbuf;
using ::std::setvbuf;
using ::std::sprintf;
using ::std::sscanf;
using ::std::tmpfile;
using ::std::tmpnam;
using ::std::tempnam;
using ::std::ungetc;
using ::std::vfprintf;
using ::std::vprintf;
using ::std::vsprintf;
#endif
