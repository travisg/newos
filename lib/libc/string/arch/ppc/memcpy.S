/*
** Copyright 2003, Fred Ferrie. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
.text

/*
** ---------------- memcpy ----------------
** input: r3 = dst mem offset (untouched)
** input: r4 = src mem offset (untouched)
** input: r5 = number of bytes (unsigned)
** note: the memory areas may not overlap
** note: destroys r5 thru r7, ctr, cr0, cr1
** ----------------------------------------
*/
.globl memcpy
memcpy:
		cmpw			r3,r4
		beqlr								; // dst and src are the same
		cmpwi			r5,0
		beqlr								; // nothing to copy
		
		li				r6,0				; // loop counter
		
		; /* count number of bytes in head */
		clrlwi.			r7,r5,30			; // clear leftmost 30 bits of r7
		beq				memcpy_copy_words	; // r7 is a number of bytes in head
		cmpw			cr1,r5,r7
		
		; /* copy head (1 to 3 bytes) by bytes */
		mtctr			r7					; // bytes in head -> count register (ctr)
memcpy_bytes_loop:
		lbzx			r7,r4,r6
		stbx			r7,r3,r6
		addi			r6,r6,1				; // next byte
		bdnz			memcpy_bytes_loop	; // ctr--; branch, if not zero
		
		; /* number of bytes < 4 ? */
		beqlr			cr1
		
memcpy_copy_words:
		; /* copy words */
		; //			current loop counter is in r6
		srawi			r5,r5,2				; // r5 is a number of words
		mtctr			r5					; // number of words -> count register (ctr)
memcpy_words_loop:
		lwzx			r7,r4,r6
		stwx			r7,r3,r6
		addi			r6,r6,4				; // next word
		bdnz			memcpy_words_loop	; // till no words left
		
		blr

