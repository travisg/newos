/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_TTY_PRIV_H
#define _NEWOS_TTY_PRIV_H

#define TTY_FLAG_CANON       0x1     /* do line editing on this endpoint */
#define TTY_FLAG_ECHO        0x2     /* echo what is sent into this one to the other endpoint */
#define TTY_FLAG_NLCR        0x4     /* translate LR to CRNL */
#define TTY_FLAG_CRNL        0x8     /* translate CR to CRNL */

#define TTY_FLAG_DEFAULT_OUTPUT TTY_FLAG_NLCR
#define TTY_FLAG_DEFAULT_INPUT  (TTY_FLAG_CANON | TTY_FLAG_ECHO | TTY_FLAG_NLCR)

struct tty_flags {
	int input_flags;
	int output_flags;
};

enum {
	_TTY_IOCTL_GET_TTY_NUM = 10000,
	_TTY_IOCTL_GET_TTY_FLAGS,
	_TTY_IOCTL_SET_TTY_FLAGS,
	_TTY_IOCTL_IS_A_TTY,
	_TTY_IOCTL_SET_PGRP
};

#endif
