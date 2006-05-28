/**
 ** Copyright 1998 Brian J. Swetland
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions, and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions, and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 ** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
 ** Distributed under the terms of the NewOS License.
 */

/*
 ** Reviewed, documented and some minor modifications and bug-fixes
 ** applied by Michael Noisternig on 2001-09-02.
 */

/*
 ** Moved to gas syntax 2005, TG
 */
/*
 ** Extended to support x86-64 long mode
 */

#define VESA_X_TARGET 640
#define VESA_Y_TARGET 480
#define VESA_BIT_DEPTH_TARGET 16

#define PGTABLE_BASE 0x8000

/* org is 0x7c00
 */
.code16
.globl _start
_start:
	jmp		start
sectors:
	.word 800                // this is interpreted as data (bytes 3 and 4)
	                         // and patched from outside to the size of the bootcode (in sectors)
start:
	cli                      // no interrupts please
	cld
	xor		%ax, %ax
	mov		%ax, %ss            // setup stack from 0 - 0x7c00
	mov		$0x7c00, %sp       // stack pointer to top of stack
	call	enable_a20       // enable a20 gate
	lgdt	%ss:gdt              // load global descriptor table
	mov		%cr0, %eax         // switch to protected mode
	or		$0x1, %al           //   by setting 'protected mode enable' bit
	mov		%eax, %cr0         //   and writing back
.code32
	.byte 0x66
	ljmp	$0x8,$(unreal32)  // do the actual switch into protected mode
    // The switch into protected mode and back is to get the processor into 'unreal' mode
    // where it is in 16-bit mode but with segments that are larger than 64k.
    // this way, the floppy code can load the image directly into memory >1Mb.
.code16
unreal32:
	mov		$0x10, %bx          // load all of the data segments with large 32-bit segment
	mov		%bx, %ds
	mov		%bx, %es
	mov		%bx, %ss
	and		$0xfe, %al          // switch back to real mode
	mov		%eax, %cr0
	ljmp	$0x0,$(unreal)   // actually go back to 16-bit

unreal:
	xor		%ax, %ax            // load NULL-descriptor (base 0) into ds, es, ss
	mov		%ax, %es            // the processor doesn't clear the internal high size bits on these descriptors
	mov		%ax, %ds            // so the descriptors can now reference 4Gb of memory, with size extensions
	mov		%ax, %ss

	// read in the second half of this stage of the bootloader
	xor		%dx, %dx            // start at head 0
	mov		$0x2, %bx           // start at sector 2 for the second half of this loader
	mov     $0x1, %cx             // one sector
	mov		$0x7e00, %edi       // right after this one
	sti
	call	load_floppy

	// read in the rest of the disk
	mov		$0x100000, %edi     // destination buffer (at 1 MB) for sector reading in load_floppy
	mov		$0x3, %bx           //   start at sector 3 (and cylinder 0)
	mov 	sectors, %cx     //   read that much sectors
	xor		%dx, %dx            //   start at head 0
	sti
	mov		$loadmsg, %si
	call	print
	call	load_floppy      // read remaining sectors at address edi
	call 	disable_floppy_motor
	mov		$okmsg, %si
	call	print

	// use int 15, ax=e820 to find the memory map
	call	find_mem_size_real

	// try to probe vesa if the shift key is held
#if 0
	in		$0x60, %al          // read data from keyboard
	and		$0x7f, %al
	cmp		$42, %al            // try to set VESA mode iff left shift key was pressed
	mov		$0, %al
	jne		no_vesa
	call	enable_vesa
no_vesa:
	mov		%al, in_vesa
#endif

	cli

	// switch back into protected mode
	mov		%cr0, %eax
	or		$1, %al
	mov		%eax,%cr0              // enter protected mode
	
.code32
	.byte 0x66
	ljmp	$0x8,$(prot_mode)  	// flush prefetch queue and enter 32-bit mode

.code16
prot_mode:
	// we're back into 16-bit protected mode

// set up a stack of long mode page tables to identity map the first 16MB of ram

	/* zero out 3 pages of ram where we're going to build some page tables */
	xor		%al, %al
	mov		$PGTABLE_BASE, %edi
	mov		$0x3000, %cx
	rep		stosb

	/* top level page table */
	mov		$((PGTABLE_BASE + 0x1000) | 0x3), %ax
	mov		%ax, PGTABLE_BASE
	mov		$((PGTABLE_BASE + 0x2000) | 0x3), %ax
	mov		%ax, (PGTABLE_BASE + 0x1000)

	/* 2MB pages to identity map the first 16MB ram */
	mov		$(0 | (1<<7) | 3), %ax
	mov		%ax, (PGTABLE_BASE + 0x2000)
	add		$0x1000, %ax
	mov		%ax, (PGTABLE_BASE + 0x2008)
	add		$0x1000, %ax
	mov		%ax, (PGTABLE_BASE + 0x2010)
	add		$0x1000, %ax
	mov		%ax, (PGTABLE_BASE + 0x2018)

	// set the page table base
	mov		$PGTABLE_BASE, %eax
	mov		%eax, %cr3
	
	// set CR4.PAE
	mov		%cr4, %eax
	bts		$5, %eax
	mov		%eax, %cr4

	// enable long mode
	// EFER.LME=1
	mov		$0xc0000080, %ecx
	rdmsr
	bts		$8, %eax
	wrmsr

	// enable paging, which will really enable long mode (CR0.PG)
	mov		%cr0, %eax
	bts		$31, %eax
	mov		%eax, %cr0

.code32
	.byte 0x66
	ljmp	$0x18,$(long_64_mode)  	// enter 64-bit mode

.code64
long_64_mode:
	// set up args to the boot image
	mov		$vesa_info, %ecx
	mov		$in_vesa, %edx
	mov		ext_mem_count, %si
	mov		ext_mem_info, %edi
	mov		$0x100000, %eax
	call	*%rax              // jump to stage1 entry

.code16
// read sectors into memory
// IN: bx = sector # to start with: should be 2 as sector 1 (bootsector) was read by BIOS
//     cx = # of sectors to read
//     edi = buffer
load_floppy:
	push	%bx
	push	%cx
tryagain:
	mov		$0x13, %al          // read a maximum of 18 sectors
	sub		%bl, %al            //   substract first sector (to prevent wrap-around ???)

	xor		%ah, %ah            // TK: don't read more then required, VMWare doesn't like that
	cmp		%cx, %ax
	jl		shorten
	mov		%cx, %ax
shorten:

	mov		%bx, %cx            //   -> sector/cylinder # to read from
	mov		$0x8000, %bx        // buffer address
	mov		$0x2, %ah           // command 'read sectors'
	push	%ax
	int		$0x13             //   call BIOS
	pop		%ax               // TK: should return number of transferred sectors in al
	                         // but VMWare 3 clobbers it, so we (re-)load al manually
	jnc		okok             //   no error -> proceed as usual
	decb	retrycnt
	jz		fail
	xor		%ah, %ah			 // reset disk controller
	int		$0x13
	jmp		tryagain		 // retry
okok:
	movb	$3, retrycnt	// reload retrycnt
	movw	$dot, %si
	call	print
	mov		$0x8000, %esi       // source
	xor		%ecx, %ecx
	mov		%al, %cl            // copy # of read sectors (al)
	shl		$0x7, %cx           //   of size 128*4 bytes
	rep addr32 movsd        //   to destination (edi) setup before func3 was called
	pop		%cx
	pop		%bx
	xor		$0x1, %dh           // read: next head
	jnz		bar6
	inc		%bh               // read: next cylinder
bar6:
	mov		$0x1, %bl           // read: sector 1
	xor		%ah, %ah
	sub		%ax, %cx            // substract # of read sectors
	jg		load_floppy      //   sectors left to read ?
	ret

disable_floppy_motor:
	xor		%al, %al
	mov		$0x3f2, %dx         // disable floppy motor
	out		%al, %dx
	ret

// prints message in reg. si
print:
	pusha
_n:
	lodsb
	or		%al, %al
	jz		_e
	mov		$0x0e, %ah
	mov		$7, %bx
	int		$0x10
	jmp		_n
_e:
	popa
	ret

// print errormsg, wait for keypress and reboot
fail:
	mov		$errormsg, %si
	call	print
	xor		%ax, %ax
	int		$0x16
	int		$0x19

// enables the a20 gate
//   the usual keyboard-enable-a20-gate-stuff
enable_a20:
	call	_a20_loop
	jnz		_enable_a20_done
	mov		$0xd1, %al
	out		%al, $0x64
	call	_a20_loop
	jnz		_enable_a20_done
	mov		$0xdf, %al
	out		%al, $0x60
_a20_loop:
	mov		$0x20000, %ecx
_loop2:
	jmp		 _c
_c:
	in		$0x64, %al
	test	$0x2, %al
	loopne	_loop2
_enable_a20_done:
	ret

loadmsg:
	.ascii	"Loading\0"
errormsg:
	.ascii	"\n\rError reading disk.\n\r\0"
okmsg:
	.ascii	"OK\n\r\0"
dot:
	.ascii	".\0"
gdt:
	// the first entry serves 2 purposes: as the GDT header and as the first descriptor
	// note that the first descriptor (descriptor 0) is always a NULL-descriptor
	.word 0xffff        // full size of GDT used
	.word gdt         //   address of GDT (dword)
	.long 0
	// descriptor 1:
	.long 0x0000ffff  // base - limit: 0 - 0xfffff * 4K
	.long 0x008f9a00  // type: 16 bit, exec-only conforming, <present>, privilege 0
	// descriptor 2:
	.long 0x0000ffff  // base - limit: 0 - 0xfffff * 4K
	.long 0x008f9200  // type: 16 bit, data read/write, <present>, privilege 0
	// descriptor 3:  long mode 64-bit code segment
	.long 0x00000000  // base & limit are ignored
	.long 0x00af9a00  // type: 64 bit, code, <present>, priviledge 0

retrycnt:
	.byte 3
ext_mem_info:
 	.long 0
ext_mem_count:
	.byte 0

.skip (510 - .)
	.word	0xaa55             // magic number for boot sector

// second sector of the boot code

.code16
	// use int 0x15, EAX=0xe820 to test for memory
	// assumes es is null
find_mem_size_real:
	mov		$0, %ebx
	mov		$0x7000, %edi		// the extended memory structures will go at 0x7000
	mov		%edi, ext_mem_info

find_mem_next:
	mov		$0xe820, %eax
	mov		$0x534d4150, %edx		// 'SMAP' in the correct order
	mov		$0x20, %ecx

	int		$0x15
	jc		done_mem_real		// if carry is set, it wasn't supported

	incb	ext_mem_count		// increment the count of the number

	add		$0x20, %edi			// increment the buffer by 0x20

	or		%ebx, %ebx				// test if we're done
	jne		find_mem_next
	
done_mem_real:
	ret

// fool around with vesa mode
enable_vesa:
	// put the VBEInfo struct at 0x30000
	mov		$0x30000, %eax
	mov		%eax, vesa_info
	mov		$0x3000, %dx
	mov		%dx, %es
	mov		$0x4f00, %ax
	mov		$0, %di
	int		$0x10

	// check the return code
	cmp		$0x4f, %al
	jne		done_vesa_bad
	cmp		$0x00, %ah
	jne		done_vesa_bad

	// check the signature on the data structure
	mov		%es:0, %eax
	cmp		$0x41534556, %eax   // 'VESA'
	je		vesa_sig_ok
	cmp		$0x32454256, %eax   // 'VBE2'
	jne		done_vesa_bad

vesa_sig_ok:
	// scan through each mode and grab the info on them
	les		%es:14, %bx	// calculate the pointer to the mode list
	mov		$0x200, %di	// push the buffer up a little to be past the VBEInfo struct

mode_loop:
	mov		%es:(%bx), %cx	// grab the next mode in the list
	cmp		$0xffff, %cx
	je		done_vesa_bad
	and		$0x01ff, %cx
	mov		$0x4f01, %ax
	int		$0x10

	// if it matches what we're looking for, go for it
	mov		%es:(%di), %ax
	test	$0x1, %ax        // test the supported bit
	jz		next_mode
	test	$0x08, %ax       // test the linear frame mode bit
	jz		next_mode
	mov		%es:18(%di), %ax
	cmp		$VESA_X_TARGET, %ax       // x
	jne		next_mode
	mov		%es:20(%di), %ax
	cmp		$VESA_Y_TARGET, %ax       // y
	jne		next_mode
	mov		%es:25(%di), %al
	cmp		$VESA_BIT_DEPTH_TARGET, %al         // bit_depth
	jne		next_mode

	// looks good, switch into it
	mov		$0x4f02, %ax
	mov		%cx, %bx
	or		$0x4000, %bx     // add the linear mode bit
	int		$0x10
	cmp		$0x004f, %ax
	je		done_vesa_good

next_mode:
	// get ready to try the next mode
	inc		%bx
	inc		%bx
	jmp		mode_loop

done_vesa_good:
	mov		$0x1, %ax
	ret

done_vesa_bad:
	xor		%ax, %ax
	ret

in_vesa:
    .byte 0
vesa_info:
	.long 0

.skip (1024 - .)
