OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc")
OUTPUT_ARCH(powerpc)

ENTRY(_start)
SEARCH_DIR("libgcc");
SECTIONS
{
	. = 0x00100000 + SIZEOF_HEADERS;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	.rodata : { *(.rodata .rodata.*) }

	/* writable data  */
	. = ALIGN(0x1000) + (. & (0x1000 - 1));
	__data_start = .;
	.data : { *(.data .gnu.linkonce.d.*) }

	__ctor_list = .;
	.ctors : { *(.ctors) }
	__ctor_end = .;

	__dtor_list = .;
	.dtors : { *(.dtors) }
	__dtor_end = .;

	/* unintialized data (in same segment as writable data) */
	__bss_start = .;
	.bss : { *(.bss) }

	. = ALIGN(0x1000);
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .dtors) }
}
