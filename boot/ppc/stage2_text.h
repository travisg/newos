/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STAGE2_TEXT_H
#define _STAGE2_TEXT_H

int s2_text_init();
void s2_putchar(char c);
void s2_puts(char *str);
int s2_printf(const char *fmt, ...);

#endif

