OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS
{
	. = 0x100000;

	/* text/read-only data */
	.text :	{
		*(.text.boot)
		*(.text* .gnu.linkonce.t.*)
	}

	.rodata : { *(.rodata .rodata.*) }
	.got : { *(.got*) }

	/* writable data  */
	/*. = ALIGN(0x1000);*/
	__data_start = .;
	.data : { *(.data .gnu.linkonce.d.* .bss) }

	/* . = ALIGN(0x1000); */
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .ctors* .dtors* .stab .stabstr .got* .debug*) }
}
