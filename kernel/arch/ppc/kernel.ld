OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc")
OUTPUT_ARCH(powerpc)

ENTRY(_start)
SEARCH_DIR("libgcc");
SECTIONS
{
	. = 0x80000000 + SIZEOF_HEADERS;

	.interp : { *(.interp) }
	.hash : { *(.hash) }
	.dynsym : { *(.dynsym) }
	.dynstr : { *(.dynstr) }
	.rel.init : { *(.rel.init) }
	.rela.init : { *(.rela.init) }
	.rel.text : { *(.rel.text) *(.rel.gnu.linkonce.t*) }
	.rela.text : { *(.rela.text) *(.rela.gnu.linkonce.t*) }
	.rel.fini : { *(.rel.fini) }
	.rela.fini : { *(.rela.fini) }
	.rel.rodata : { *(.rel.rodata) *(.rel.gnu.linkonce.r*) }
	.rela.rodata : { *(.rela.rodata) *(.rela.gnu.linkonce.r*) }
	.rel.data : { *(.rel.data) *(.rel.gnu.linkonce.d*) }
	.rela.data : { *(.rela.data) *(.rela.gnu.linkonce.d*) }
	.rel.ctors : { *(.rel.ctors) }
	.rela.ctors : { *(.rela.ctors) }
	.rel.dtors : { *(.rel.dtors) }
	.rela.dtors : { *(.rela.dtors) }
	.rel.sdata : { *(.rel.sdata) }
	.rela.sdata : { *(.rela.sdata) }
	.rel.got : { *(.rel.got) }
	.rela.got : { *(.rela.got) }
	.rel.bss : { *(.rel.bss) }
	.rela.bss : { *(.rela.bss) }
	.rel.plt : { *(.rel.plt) }
	.rela.plt : { *(.rela.plt) }
	.init : { *(.init) }
	.plt : { *(.plt) }

	/* text/read-only data */
	.text :	{ *(.text) *(.text.*) *(.stub) *(.gnu.linkonce.t.*) } = 0
	.fini : { *(.fini) }

	.rodata : { *(.rodata) *(.rodata.*) *(.gnu.linkonce.r.*) }
	.rodata1 : { *(.rodata1) }

	/* writable data  */
	. = ALIGN(0x1000);
	__data_start = .;
	.data : { *(.data .gnu.linkonce.d.*) }

	__ctor_list = .;
	.ctors : { *(.ctors) }
	__ctor_end = .;
	__dtor_list = .;
	.dtors : { *(.dtors) }
	__dtor_end = .;
	.got : { *(.got.plt) *(.got) }
	.dynamic : { *(.dynamic) }
	.sdata : { *(.sdata) *(.sdata.*) *(.gnu.linkonce.s.*) }

	.sbss : { *(.dynsbss) *(.sbss) *(.sbss.*) *(.gnu.linkonce.sb.*) *(.scommon) }

	/* unintialized data (in same segment as writable data) */
	__bss_start = .;
	.bss : { *(.dynbss) *(.bss) *(.bss.*) *(.gnu.linkonce.b.*) *(COMMON) }

	. = ALIGN(0x1000);
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : { *(.comment .note .eh_frame) }
}
