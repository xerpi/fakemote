OUTPUT_FORMAT("elf32-bigarm")
OUTPUT_ARCH(arm)
ENTRY(_start)

/* Sections area */
MEMORY {
	table		: ORIGIN = 0x0, LENGTH = 0x4000
	exe(rx)		: ORIGIN = 0x13970000, LENGTH = 0xB000
	data(rw)	: ORIGIN = 0x1397B000, LENGTH = 0xE000
}

SECTIONS {
	.ios_info_table : {
		KEEP (*(.ios_info_table))
	} > table

	.init : {
		*(.init)
		. = ALIGN(4);
	} > exe

	.text : {
		*(.text*)
		*(.gnu.warning)
		*(.gnu.linkonce.t.*)
		*(.init)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} > exe

	.rodata : {
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		. = ALIGN(4);
	} > exe

	.data : {
		*(.data*)
		*(.data.*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(4);
	} > data

	.bss (NOLOAD) : {
		*(.dynsbss)
		*(.gnu.linkonce.sb.*)
		*(.bss*)
		*(COMMON)
		KEEP(*(.ios_bss))
		. = ALIGN(4);
	} > data
}
