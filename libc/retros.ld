/*
 * Programs built on the C library with GNU ld (program.mk): a flat binary at 0x1000000, where RetrOS loads them.
 * RetrOS starts a program at its first byte, so crt1.o's _start, in its own section, comes first. .bss is in the
 * file, as zeros, because RetrOS maps only what's in the file.
 */
OUTPUT_FORMAT("binary")
ENTRY(_start)
SECTIONS
{
    . = 0x1000000;

    .text : {
        *(.text.retros_entry)
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    .init_array : ALIGN(4) {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .fini_array : ALIGN(4) {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }

    .data : ALIGN(16) {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
    }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
