OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
	.text : {
		*(.text .gnu.linkonce.t.* .data* .gnu.linkonce.d.* .rodata*)
	}

	.bss : {
		*(.bss)
		*(.common)
	}

	end = .; _end = .;

	/DISCARD/ : {
		*(.eh_frame .eh_frame_hdr .debug* .note* .comment* .gnu.version* .stab .stabstr .ctors .dtors .fini* .init* .line .preinit_array)
	}
}
