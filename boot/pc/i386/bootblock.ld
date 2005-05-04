OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS
{
	. = 0x7c00; 

	/* one big section */
	.bootblock : { *(.text .rodata .data .bss) }

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .dtors .stab .stabstr .debug*) }
}
