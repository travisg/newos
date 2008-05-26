.code32

#define LOAD_ADDRESS 0x100000 // 1MB
#define PGTABLE_BASE 0x8000

.global _start
_start:
	jmp		_enter64

#define MB_MAGIC	0x1BADB002
#define MB_FLAGS	(1<<0 | 1<<1 | 1<<16)				// page align, meminfo, rest of header
#define MB_CHECKSUM (-(MB_MAGIC + MB_FLAGS))

.align 4
multiboot:
.long	MB_MAGIC		// magic
.long	MB_FLAGS		// flags
.long 	MB_CHECKSUM		// checksum
	// address part of the header
.long	multiboot		// address of header
.long	LOAD_ADDRESS	// load address of the binary
.long	_end			// end of all the text + data
.long	0				// no bss, linker script combines with data
.long	_start			// entry point

_enter64:
	// zero out 3 pages of ram where we're going to build some page tables
	xor     %eax, %eax
	mov     $PGTABLE_BASE, %edi
	mov     $0x3000, %ecx
	rep     stosb

	// top level page table
	mov     $((PGTABLE_BASE + 0x1000) | 0x3), %eax
	mov     %eax, PGTABLE_BASE
	mov     $((PGTABLE_BASE + 0x2000) | 0x3), %eax
	mov     %eax, (PGTABLE_BASE + 0x1000)

	// 2MB pages to identity map the first 16MB ram
	mov     $(0 | (1<<7) | 3), %eax
	mov     %eax, (PGTABLE_BASE + 0x2000)
	add     $0x200000, %eax
	mov     %eax, (PGTABLE_BASE + 0x2008)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2010)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2018)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2020)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2028)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2030)
	add     $0x200000, %eax        
	mov     %eax, (PGTABLE_BASE + 0x2038)

	// set the page table base
	mov     $PGTABLE_BASE, %eax
	mov     %eax, %cr3

	// load the 64bit gdt
	lgdt	gdt64

	// set CR4.PAE
	mov     %cr4, %eax          
	bts     $5, %eax
	mov     %eax, %cr4        

	// enable long mode      
	// EFER.LME=1            
	mov     $0xc0000080, %ecx
	rdmsr   
	bts     $8, %eax             
	wrmsr  

	// enable paging, which will really enable long mode (CR0.PG)
	mov     %cr0, %eax
	bts     $31, %eax
	mov     %eax, %cr0

	ljmp	$0x08,$(long_64_mode)

.code64
long_64_mode:
	// print an a
	mov		$0xb8000, %rax
	movw	$0x0f61, 0(%rax)

	// set up the stack
	mov		$stack_end, %rsp

	// print a b
	mov		$0xb8000, %rax
	movw	$0x0f62, 2(%rax)

	// ebx should contain a pointer to the multiboot info structure
	mov		%rbx,%rdi

	jmp		stage1_main

.align 4
gdt64:
	// first entry, also the gdt descriptor
	.word 0xffff
	.long gdt64
	.word 0
	// entry 64bit code segment
    .long 0x00000000  // base & limit are ignored
    .long 0x00af9a00  // type: 64 bit, code, <present>, priviledge 0


.data
.align 8
stack:
	.skip 0x4000
stack_end:
