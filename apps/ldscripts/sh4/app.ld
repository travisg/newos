OUTPUT_FORMAT("elf32-shl", "elf32-shl", "elf32-shl")
OUTPUT_ARCH(sh)

ENTRY(__start)
SEARCH_DIR("libgcc");
SECTIONS
{
	. = 0x00200000 + SIZEOF_HEADERS;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	___ctor_list = .;
	.ctors : { *(.ctors) }
	___ctor_end = .;

  	.rodata :
	{ 
 		*(.rodata)
 		. = ALIGN(0x1000);
 	} =0x9000

	/* writable data  */
  	. = ALIGN(0x1000) + (. & (0x1000 - 1));
	___data_start = .;
	.data : { *(.data .gnu.linkonce.d.*) }

	/* unintialized data (in same segment as writable data) */
	___bss_start = .;
	.bss : { *(.bss) }

	. = ALIGN(0x1000);
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .dtors) }
}
