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
     * Clear .bss. In the flat kernel image it's the zeros at the end,
     * and the bootloader doesn't always load all of them from the disk.
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
