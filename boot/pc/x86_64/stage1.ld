OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)
SECTIONS
{
	. = 0x100000;

	/* text/read-only data */
	.text :	{ *(.text .gnu.linkonce.t.*) }

	.rodata : { *(.rodata .rodata.*) }

	/* writable data  */
	.data : { *(.data .gnu.linkonce.d.* .got* .bss) }

	/* payload */
	. = ALIGN(0x1000);
	_payload_start = .;
	.payload : { *(.payload) }
	_payload_end = .;

	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame .ctors .dtors .stab .stabstr .debug*) }
}
