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

; /*
; ** Reviewed, documented and some minor modifications and bug-fixes
; ** applied by Michael Noisternig on 2001-09-02.
; */

SECTION
CODE16
ORG 0x7c00               ; start code at 0x7c00
	jmp short start
	mov cx,0x1           ; this is interpreted as data (bytes 3 and 4)
	                     ; and patched from outside to the size of the bootcode (in sectors)
start:
	cli                  ; no interrupts please
	cld
	xor ax,ax
	mov ss,ax            ; setup stack from 0 - 0x7c00
	mov sp,0x7c00        ; stack pointer to top of stack
	call enable_a20      ; enable a20 gate
	lgdt [ss:gdt]        ; load global descriptor table
	mov eax,cr0          ; switch to protected mode
	or al,0x1            ;   by setting 'protected mode enable' bit
	mov cr0,eax          ;   and writing back
	jmp dword 0x8:a      ; do the actual switch into protected mode
    ; The switch into protected mode and back is to get the processor into 'unreal' mode
    ; where it is in 16-bit mode but with segments that are larger than 64k.
    ; this way, the floppy code can load the image directly into memory >1Mb.
a:
	mov bx,0x10          ; load all of the data segments with large 32-bit segment
	mov ds,bx
	mov es,bx
	mov ss,bx
	and al,0xfe          ; switch back to real mode
	mov cr0,eax
	jmp 0x7c0:b-0x7c00   ; actually go back to 16-bit
b:
	xor ax,ax            ; load NULL-descriptor (base 0) into ds, es, ss
	mov es,ax            ; the processor doesn't clear the internal high size bits on these descriptors
	mov ds,ax            ; so the descriptors can now reference 4Gb of memory, with size extensions
	mov ss,ax
	mov edi,0x100000     ; destination buffer (at 4 MB) for sector reading in func3
	mov bx,0x2           ;   start at sector 2 (and cylinder 0)
	mov cx,[cs:0x2]      ;   read that much sectors
	xor dx,dx            ;   start at head 0
	sti
	call func3           ; read remaining sectors at address edi
	cli
	call find_mem_size

	mov [0x4000],eax     ; save memory size at that address
	mov dword [0x4004],0x10000  ; address of page table directory -> see below

	mov	ecx,1024         ; # entries in page table directory
	mov edi,[0x4004]     ; set base of page table directory to 0x10000 (1 MB)
	xor eax,eax
	rep a32 stosd		; clear out the page tables/page directory
	
	mov ebx,[0x4004]	; get the page table base
	mov	ecx,1024		; map the first 4 MB
	mov eax,0x3         ; pages are present and writable
	mov edi,0x1000      ; start right after the page table directory
bar2:					; set up the page tables
	mov [ebx+edi],eax
	add eax,0x1000
	add edi,byte +0x4
	loop bar2

	mov	ecx,0x1          ; page dir entry 0 -> first page table
	mov eax,[0x4004]
	add eax,0x1003       ; page table right after page dir, present and writable
	xor edi,edi
bar4:                    ; set up the page directory
	mov [ebx+edi],eax
	add eax,0x1000
	add edi,byte +0x4
	loop bar4

	mov eax,[0x4004]
	mov ecx,eax
	or al,0x3
	mov [ebx+0xffc],eax   ; last page dir entry -> page dir (thus itself)
	mov ebx,[dword 0x100074]  ; load dword at rel. address 0x74 from read-destination-buffer
	add ebx,0x101000      ; for stage2 entry
	mov ebp,0x100000      ; stack base pointer -> read-buffer
	mov al,0xcf
	mov [ds:gdt+14],al    ; set desc. 1 and 2
	mov [ds:gdt+22],al    ;   to 32-bit segment
	lgdt [ds:gdt]         ; load global descriptor table
	mov cr3,ecx           ; load page table directory
	mov eax,0x80000001
	mov cr0,eax           ; re-enter protected mode with paging enabled
	jmp dword 0x8:code32  ; flush prefetch queue
code32:
BITS 32
	mov		ax,0x10       ; load descriptor 2 in all segment selectors (except cs)
	mov		ds,ax
	mov		es,ax
	mov		fs,ax
	mov		gs,ax
	mov		ss,ax
	mov		esp,ecx
	sub		esp,byte 4    ; esp points below page table dir !?
	mov		edx,0x7c04    ; edx points to label 'start' !?
	push	ebp           ; ebp points to our read-destination buffer !?
	push	edx           ; save &'start'
	push	dword [word 0x4000]  ; save memory size
	call	ebx           ; jump to stage2 entry
inf:jmp		short inf

BITS 16
gdt:
	; the first entry serves 2 purposes: as the GDT header and as the first descriptor
	; note that the first descriptor (descriptor 0) is always a NULL-descriptor
	db 0xFF        ; full size of GDT used
	db 0xff        ;   which means 8192 descriptors * 8 bytes = 2^16 bytes
	dw gdt         ;   address of GDT (dword)
	dd 0
	; descriptor 1:
	dd 0x0000ffff  ; base - limit: 0 - 0xfffff * 4K
	dd 0x008f9a00  ; type: 16 bit, exec-only conforming, <present>, privilege 0
	; descriptor 2:
	dd 0x0000ffff  ; base - limit: 0 - 0xfffff * 4K
	dd 0x008f9200  ; type: 16 bit, data read/write, <present>, privilege 0

; read sectors into memory
; IN: bx = sector # to start with: should be 2 as sector 1 (bootsector) was read by BIOS
;     cx = # of sectors to read
;     edi = buffer
func3:
	push bx
	push cx
	mov al,0x13          ; read a maximum of 18 sectors
	sub al,bl            ;   substract first sector (to prevent wrap-around ???)
	mov cx,bx            ;   -> sector/cylinder # to read from
	mov bx,0x8000        ; buffer address
	mov ah,0x2           ; command 'read sectors'
	int 0x13             ;   call BIOS
	jc spin              ;   error -> loop infinitely in spin
	; NOTE: errors on a floppy may be due to the motor failing to spin up quickly enough;
	;       the read should be retried at least three times, resetting the disk with AH=00h between attempts
	mov esi,0x8000       ; source
	xor ecx,ecx
	mov cl,al            ; copy # of read sectors (al)
	shl cx,0x7           ;   of size 128*4 bytes
	rep a32 movsd        ;   to destination (edi) setup before func3 was called
	pop cx
	pop bx
	xor dh,0x1           ; read: next head
	jnz bar6
	inc bh               ; read: next cylinder
bar6:
	mov bl,0x1           ; read: sector 1
	xor ah,ah
	sub cx,ax            ; substract # of read sectors
	jg func3             ;   sectors left to read ?
	mov al,0xc           ; reset controller and enable DMA on floppy 0 (drive A)
	mov dx,0x3f2         ;   (why only on drive A ? and why do that now ?)
	out dx,al
	ret
spin:
	jmp short spin

; find memory size by testing
; OUT: eax = memory size
find_mem_size:
	mov ax,0x3132        ; test value
	mov ebx,0x100ff0     ; start above conventional mem + HMA = 1 MB + 1024 Byte
_fms_loop:
	mov dx,[ebx]         ; read value
	mov [ebx],ax         ;   write test value
	mov di,[ebx]         ;   read it again
	mov [ebx],dx         ; write back old value
	cmp di,ax
	jnz _fms_loop_out    ; read value != test value -> above mem limit
	add ebx,0x1000       ; test next page (4 K)
	jmp short _fms_loop
_fms_loop_out:
	mov eax,ebx
	sub eax,0x1000
	add eax,byte +0x10
	ret

; enables the a20 gate
;   the usual keyboard-enable-a20-gate-stuff
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

	times 510-($-$$) db 0  ; filler for boot sector
	dw	0xaa55             ; magic number for boot sector
