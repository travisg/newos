; **
; ** Copyright 1998 Brian J. Swetland
; ** All rights reserved.
; **
; ** Redistribution and use in source and binary forms, with or without
; ** modification, are permitted provided that the following conditions
; ** are met:
; ** 1. Redistributions of source code must retain the above copyright
; **    notice, this list of conditions, and the following disclaimer.
; ** 2. Redistributions in binary form must reproduce the above copyright
; **    notice, this list of conditions, and the following disclaimer in the
; **    documentation and/or other materials provided with the distribution.
; ** 3. The name of the author may not be used to endorse or promote products
; **    derived from this software without specific prior written permission.
; **
; ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
; ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
; ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
; ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
; ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
; ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
; ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
; ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
; ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; /* 
; ** Copyright 2001, Travis Geiselbrecht. All rights reserved.
; ** Distributed under the terms of the NewOS License.
; */
SECTION
CODE16
ORG 0x7c00
	jmp short start
	mov cx,0x1

start:
	cli
	xor ax,ax
	mov ss,ax
	mov sp,0x7c00
	call enable_a20
	lgdt [ss:gdt]
	mov eax,cr0
	or al,0x1
	mov cr0,eax
	jmp dword 0x8:a
a:
	mov bx,0x10
	mov ds,bx
	mov es,bx
	mov ss,bx
	and al,0xfe
	mov cr0,eax
	jmp 0x7c0:b-0x7c00
b:
	xor ax,ax
	mov es,ax
	mov ds,ax
	mov ss,ax
	mov edi,0x100000
	mov bx,0x2
	mov cx,[cs:0x2]
	xor dx,dx
	sti
	call func3
	cli
	call find_mem_size
	mov [0x4000],eax
	mov dword [0x4004],0x10000
;	mov ecx,eax
;	shr ecx,0xc
	mov	ecx,0x1024
	mov edi,[0x4004]
;	add ecx,0x400
	xor eax,eax
	rep a32 stosd		; clear out the page tables

	mov ebx,[0x4004]	; get the page table base
;	mov ecx,[0x4000]	; get the size of memory
;	shr ecx,0xc			; number of pages
	mov	ecx,0x1024		; map the first 4 MB
	mov eax,0x3
	mov edi,0x1000
bar2:					; set up the page tables
	mov [ebx+edi],eax
	add eax,0x1000
	add edi,byte +0x4
	loop bar2

;	mov ecx,[0x4000]
;	shr ecx,0x16
;	inc ecx
	mov	ecx,0x1
	mov eax,[0x4004]
	add eax,0x1003
	xor edi,edi
bar4:					; set up the page directory
	mov [ebx+edi],eax
	add eax,0x1000
	add edi,byte +0x4
	loop bar4

	mov eax,[0x4004]
	mov ecx,eax
	or al,0x3
	mov [ebx+0xffc],eax
	mov ebx,[dword 0x100074]
	add ebx,0x101000
	mov ebp,0x100000
	mov al,0xcf
	mov [ds:gdt+14],al
	mov [ds:gdt+22],al
	lgdt [ds:gdt]
	mov cr3,ecx
	mov eax,0x80000001
	mov cr0,eax
	jmp dword 0x8:code32
code32:
; from now on, nothing makes any sense, we're in 32 bit mode now
	mov eax,0x8e660010
	fsub dword [bp-0x72]
	shl byte [bp-0x72],0xe0
	mov gs,eax
	mov ss,eax
	mov sp,cx
	sub sp,byte +0x4
	mov dx,0x7c04
	add [bx+si],al
	push bp
	push dx
	push word [esi]
	add [bx+si-0x1],al
	shr bx,cl
	db 0xFE
gdt:
	db 0xFF
	db 0xff
	dw gdt
	dd 0
	dd 0x0000ffff
	dd 0x008f9a00
	dd 0x0000ffff
	dd 0x008f9200

func3:
	push bx
	push cx
	mov al,0x13
	sub al,bl
	mov cx,bx
	mov bx,0x8000
	mov ah,0x2
	int 0x13
	jc spin
	mov esi,0x8000
	xor ecx,ecx
	mov cl,al
	shl cx,0x7
	rep a32 movsd
	pop cx
	pop bx
	xor dh,0x1
	jnz bar6
	inc bh
bar6:
	mov bl,0x1
	xor ah,ah
	sub cx,ax
	jg func3
	mov al,0xc
	mov dx,0x3f2
	out dx,al
	ret
spin:
	jmp short spin

find_mem_size:
	mov ax,0x3132
	mov ebx,0x100ff0
_fms_loop:
	mov dx,[ebx]
	mov [ebx],ax
	mov di,[ebx]
	mov [ebx],dx
	cmp di,ax
	jnz _fms_loop_out
	add ebx,0x1000
	jmp short _fms_loop
_fms_loop_out:
	mov eax,ebx
	sub eax,0x1000
	add eax,byte +0x10
	ret

enable_a20:
	call _a20_loop
	jnz _enable_a20_done
	mov al,0xd1
	out 0x64,al
	call _a20_loop
	jnz _enable_a20_done
	mov al,0xdf
	out 0x60,al
_a20_loop:
	mov ecx,0x20000
_loop2:
	jmp short _c
_c:
	in al,0x64
	test al,0x2
	loopne _loop2
_enable_a20_done:
	ret
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	push bp
	stosb
