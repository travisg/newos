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

