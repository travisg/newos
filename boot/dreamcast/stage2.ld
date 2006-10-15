OUTPUT_FORMAT("elf32-shl", "elf32-shl", "elf32-shl")
OUTPUT_ARCH(sh)
 
ENTRY(__start)
SECTIONS
{
	/* account for the 1 page of stage1 and 4 pages of bootdir before the binary */
	. = 0x8c010000 + 0x5000 + SIZEOF_HEADERS;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	.rodata : { *(.rodata .rodata.*) }

	/* writable data  */
/*	. = ALIGN(0x1000);*/
	__ctor_list = .;
	.ctors : { *(.ctors) }
	__ctor_end = .;

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
