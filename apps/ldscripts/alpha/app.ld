OUTPUT_FORMAT("elf64-alpha", "elf64-alpha", "elf64-alpha")
OUTPUT_ARCH(alpha)

ENTRY(_start)
SEARCH_DIR("libgcc");
SECTIONS
{
/*	. = 0x00100000 + SIZEOF_HEADERS; */
	. = 0x00100000 + 0x80;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	__ctor_list = .;
	.ctors : { *(.ctors) }
	__ctor_end = .;

	.rodata : { *(.rodata) }

	/* writable data  */
	. = ALIGN(0x1000);
	__data_start = .;
	.data : { *(.data .gnu.linkonce.d.*) }

	/* unintialized data (in same segment as writable data) */
	__bss_start = .;
	.bss : { *(.bss) }

	. = ALIGN(0x1000);
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .dtors) }
}
