// expects a stack page like this:
// (stack has to be at (0x9e000)
//	0x9effc            : final esp
//  0x9eff8            : page dir
//
//  0x9e000 - 0x9e006  : gdt descriptor
//	0x9e008 - 0x9e020  : gdt
//
// smp_trampoline must be located at 0x9f000
.globl smp_trampoline
.globl smp_trampoline_end
.globl foo

.code16
smp_trampoline:
	cli
	
	mov    $0x9e00,%ax      # load the data segment
	mov    %ax,%ds

	// display a 'a' to the serial console
1:
	mov    $0x3fd,%dx
	inb    %dx,%al
	test   $0x64,%al
	je     1b
	mov    $'a',%al
	mov    $0x3f8,%dx
	outb   %al,%dx

.byte 0x66, 0x0f, 0x01, 0x15, 0x00, 0xe0, 0x09, 0x00
//	lgdt   0x9e000          # load the gdt
	
	// display a 'b' to the serial console
1:
	mov    $0x3fd,%dx
	inb    %dx,%al
	test   $0x64,%al
	je     1b
	mov    $'b',%al
	mov    $0x3f8,%dx
	outb   %al,%dx
	
	movl   %cr0,%eax
	orl    $0x01,%eax
	movl   %eax,%cr0              # switch into protected mode

.code32
_trampoline_32:
	.byte 0x66
	ljmp   $0x08,$(trampoline_32 - smp_trampoline + 0x9f000)
trampoline_32:
	mov    $0x10, %ax
	mov    %ax, %ds
	mov    %ax, %es
	mov    %ax, %fs
	mov    %ax, %gs
	mov    %ax, %ss

	// display a 'c' to the serial console
1:
	mov    $0x3fd,%dx
	inb    %dx,%al
	test   $0x64,%al
	je     1b
	mov    $'c',%al
	mov    $0x3f8,%dx
	outb   %al,%dx

	movl   $0x9eff8,%esp    # set up the stack pointer

	popl   %eax             # get the page dir
	movl   %eax,%cr3        # set the page dir
	
	popl   %eax             # get the final stack location
	movl   %eax,%esp

	// display a 'd' to the serial console
1:
	mov    $0x3fd,%dx
	inb    %dx,%al
	test   $0x64,%al
	je     1b
	mov    $'d',%al
	mov    $0x3f8,%dx
	outb   %al,%dx

	movl   $trampoline_after_paging,%ecx

	movl   %cr0,%eax
	orl    $0x80000000,%eax
	movl   %eax,%cr0          # enable paging

	jmp    *%ecx
trampoline_after_paging:
	// display a 'e' to the serial console
1:
	mov    $0x3fd,%dx
	inb    %dx,%al
	test   $0x64,%al
	je     1b
	mov    $'e',%al
	mov    $0x3f8,%dx
	outb   %al,%dx

	ret

foo:
	.byte 0x00,0x00,0x00,0x00,0x00,0x00

smp_trampoline_end:

