/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _PRINTF_H_
#define _PRINTF_H_

#include <libc/stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int sprintf(char * buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif

