/*
 * ip20.ld
 *	Link ip20mon at 0x88002000 (physical 0x08002000: the second page of
 *	RAM, where NetBSD's IP2x boot loader also loads).  Text and data go
 *	in separate segments so elf2ecoff can produce ECOFF .text and .data
 *	sections for the PROM.
 */

OUTPUT_FORMAT("elf32-tradbigmips")
OUTPUT_ARCH(mips)
ENTRY(start)

PHDRS
{
	text PT_LOAD FLAGS(5);		/* R X */
	data PT_LOAD FLAGS(6);		/* R W */
}

SECTIONS
{
	. = 0x88002000;

	.text : {
		*(.text .text.*)
	} :text

	.rodata : {
		*(.rodata .rodata.*)
	} :text

	. = ALIGN(16);
	.data : {
		*(.data .data.*)
		*(.sdata .sdata.*)
	} :data

	. = ALIGN(16);
	_fbss = .;
	.bss : {
		*(.sbss .sbss.*)
		*(.scommon)
		*(.bss .bss.*)
		*(COMMON)
	} :data
	. = ALIGN(16);
	_end = .;

	/DISCARD/ : {
		*(.MIPS.abiflags)
		*(.reginfo)
		*(.pdr)
		*(.eh_frame)
		*(.comment)
		*(.note*)
		*(.gnu.attributes)
		*(.mdebug*)
	}
}
