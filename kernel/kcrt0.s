.code32
.section .text.prologue

.global _start
_start:
    popl %eax

    finit

    movl $stack, %esp

    pushl %esp
    pushl %eax
    cli

    /**
     * Clear .bss. In the flat kernel image it's the zeros at the end.
     * The bootloader reads exactly the image's sectors (#32), but .bss
     * mustn't depend on what was in memory or on the disk past them.
     * The stack is in .data, so the values pushed above are kept.
     */
    movl $_bss_s, %edi
    movl $_bss_size, %ecx
    xorl %eax, %eax
    cld
    rep stosb

    call kernel

.section .data
.align 16
stack_bottom:
.skip 4096 # 4 KiB
stack:
