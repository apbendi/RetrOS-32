/*
 * The start of every program built on the C library.
 *
 * RetrOS starts a program at its first byte, with argc in %ebx and argv in %ecx. It starts the program's threads
 * there too, with the thread's function in %edx and its argument in %ebx. _start is in a section of its own, which
 * the linkers put first: TCC because crt1.o is the first file it loads (apps/tcc/README.md), GNU ld by libc/retros.ld.
 */
.section .text.retros_entry, "ax"
.global _start
.type _start, @function
_start:
	xorl %ebp, %ebp
	andl $-16, %esp
	subl $8, %esp
	testl %edx, %edx
	jnz 1f

	pushl %ecx		/* argv */
	pushl %ebx		/* argc */
	call __retros_start_main

1:	pushl %ecx
	pushl %ebx		/* the thread's argument */
	call *%edx
	call __retros_thread_exit
