/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define GLOBAL(x) \
	.globl x; \
x:

#define EXTERN(x) \
	.globl x


.text


GLOBAL(__RLD_STARTUP)
	jmp rld0
	
	.align 16
GLOBAL(__RLD_HELPERS)
	/*
	 * probably will never be used, uspa struct is better idea
	 */

	.align 256
GLOBAL(rld0)
	mov	4(%esp), %eax
	pushl	%eax
	call	rldmain



/*
 * external dependencies
 */
EXTERN(rldmain)
	
