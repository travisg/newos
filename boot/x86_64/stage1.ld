OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)
SECTIONS
{
	. = 0x100000;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	__ctor_list = .;
	.ctors : { *(.ctors) }
	__ctor_end = .;

	.rodata : { *(.rodata .rodata.*) }

	/* writable data  */
	. = ALIGN(0x1000);
	__data_start = .;
	.data : { *(.data .gnu.linkonce.d.* .bss) }

	/* . = ALIGN(0x1000); */
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .dtors .stab .stabstr .debug*) }
}
