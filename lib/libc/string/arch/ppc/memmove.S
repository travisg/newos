/*
** Copyright 2003, Fred Ferrie. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
.text

/*
** ----------------- memmove -------------------
** input: r3 = dst mem offset (untouched)
** input: r4 = src mem offset (untouched)
** input: r5 = number of bytes (unsigned)
** note: overlap protection provided
** note: destroys r5, r6, r7, ctr, cr0, cr1, cr2
** ---------------------------------------------
*/
.globl memmove
memmove:
		cmpw			cr1,r3,r4
		beqlr			cr1					; // dst and src are the same
		cmpwi			r5,0
		beqlr								; // nothing to move
		
		; /* count number of bytes in head/tail */
		clrlwi.			r7,r5,30
		cmpw			cr2,r5,r7
		
		; /* moving up or down? */
		blt				cr1,memmove_moving_down
		
		; /* moving up (dst < src) */
		subi			r6,r5,1				; // init a loop counter
		; // if there is no bytes in tail, move words only
		beq				moving_up_move_words
		
		; /* move tail (1 to 3 bytes) by bytes */
		mtctr			r7					; // bytes in tail -> count register (ctr)
moving_up_bytes_loop:
		lbzx			r7,r4,r6
		stbx			r7,r3,r6
		subi			r6,r6,1
		bdnz			moving_up_bytes_loop
		
		; /* number of bytes < 4 ? */
		beqlr			cr2
		
moving_up_move_words:
		; /* move words up */
		subi			r6,r6,3				; // patch a loop counter
		srawi			r5,r5,2				; // r5 is a number of words
		mtctr			r5					; // number of words -> count register (ctr)
moving_up_words_loop:
		lwzx			r7,r4,r6
		stwx			r7,r3,r6
		subi			r6,r6,4
		bdnz			moving_up_words_loop
		
		blr
		
; /* moving down (src < dst), acts like memcpy */
memmove_moving_down:
		li				r6,0				; // init a loop counter
		; // if there is no bytes in head, move words only
		beq				moving_down_move_words
		
		; /* move head (1 to 3 bytes) by bytes */
		mtctr			r7					; // bytes in head -> count register (ctr)
moving_down_bytes_loop:
		lbzx			r7,r4,r6
		stbx			r7,r3,r6
		addi			r6,r6,1
		bdnz			moving_down_bytes_loop
		
		; /* number of bytes < 4 ? */
		beqlr			cr2
		
moving_down_move_words:
		; /* move words down */
		; //			current loop counter is in r6
		srawi			r5,r5,2				; // r5 is a number of words
		mtctr			r5					; // number of words -> count register (ctr)
moving_down_words_loop:
		lwzx			r7,r4,r6
		stwx			r7,r3,r6
		addi			r6,r6,4
		bdnz			moving_down_words_loop
		
		blr

