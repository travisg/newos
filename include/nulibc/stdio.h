/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__nulibc_stdio__hh__
#define __newos__nulibc_stdio__hh__


#include <types.h>
#include <nulibc/stdarg.h>


#ifdef __cplusplus
extern "C"
{
#endif

struct FILE;	/* declare as opaque type */
typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;


int printf(char const *format, ...);
int fprintf(FILE *stream, char const *format, ...);
int sprintf(char *str, char const *format, ...);
int snprintf(char *str, size_t size, char const *format, ...);
int asprintf(char **ret, char const *format, ...);
int vprintf(char const *format, va_list ap);
int vfprintf(FILE *stream, char const *format, va_list ap);
int vsprintf(char *str, char const *format, va_list ap);
int vsnprintf(char *str, size_t size, char const *format, va_list ap);
int vasprintf(char **ret, char const *format, va_list ap);

FILE *fopen(char const *, char const *);
int  *fflush(FILE *);
int  *fclose(FILE *);

int   feof(FILE *);
char *fgets(char *, int, FILE *);

int scanf(char const *format, ...);
int fscanf(FILE *stream, char const *format, ...);
int sscanf(char const *str, char const *format, ...);
int vscanf(char const *format, va_list ap);
int vsscanf(char const *str, char const *format, va_list ap);
int vfscanf(FILE *stream, char const *format, va_list ap);

#ifdef __cplusplus
}
#endif


#endif
