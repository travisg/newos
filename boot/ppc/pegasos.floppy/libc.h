#ifndef __LIBC_H
#define __LIBC_H

#include "types.h"

void puts(char *str);
void putchar(char c);
void write_hex(unsigned int val);

void *memcpy(void *to, const void *from, size_t len);

#endif
