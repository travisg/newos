/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define FUNCTION(name) .align 2 ; .globl name ; name

FUNCTION(fabs):
	bx	lr
