/*
	ISR entry point, followed tutorial:
	http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
*/

#include <arch/gdt.h>

.code32
.global idt_flush
idt_flush:
	movl 4(%esp), %eax
	lidt (%eax)
	ret

/*
	Both stubs leave the same frame (struct registers): interrupt number, error code, then ebp
	pushed like a function prologue, right below the eip pushed by the CPU. ebp points at it,
	so a backtrace from ebp starts at the interrupted instruction.
*/
.macro ISR_NO_ERR index
	.global isr\index
	isr\index:
		cli
    pushl %ebp
    movl %esp, %ebp
		push $0
		push $\index
		jmp isr_entry
.endm

/* The CPU has already pushed an error code. Swap it with ebp, so it ends up above ebp as in ISR_NO_ERR. */
.macro ISR_ERR index
	.global isr\index
	isr\index:
		cli
    xchgl %ebp, (%esp)
    pushl %ebp
    leal 4(%esp), %ebp
		push $\index
		jmp isr_entry
.endm

ISR_NO_ERR 0
ISR_NO_ERR 1
ISR_NO_ERR 2
ISR_NO_ERR 3
ISR_NO_ERR 4
ISR_NO_ERR 5
ISR_NO_ERR 6
ISR_NO_ERR 7
ISR_ERR 8
ISR_NO_ERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NO_ERR 15
ISR_NO_ERR 16
ISR_ERR 17
ISR_NO_ERR 18
ISR_NO_ERR 19
ISR_NO_ERR 20
ISR_ERR 21
ISR_NO_ERR 22
ISR_NO_ERR 23
ISR_NO_ERR 24
ISR_NO_ERR 25
ISR_NO_ERR 26
ISR_NO_ERR 27
ISR_NO_ERR 28
ISR_NO_ERR 29
ISR_NO_ERR 30
ISR_NO_ERR 31
ISR_NO_ERR 32
ISR_NO_ERR 33
ISR_NO_ERR 34
ISR_NO_ERR 35
ISR_NO_ERR 36
ISR_NO_ERR 37
ISR_NO_ERR 38
ISR_NO_ERR 39
ISR_NO_ERR 40
ISR_NO_ERR 41
ISR_NO_ERR 42
ISR_NO_ERR 43
ISR_NO_ERR 44
ISR_NO_ERR 45
ISR_NO_ERR 46
ISR_NO_ERR 47

isr_entry:
  cli

  pushal

  pushl	%ds
  pushl	$16
  call	load_data_segments
  addl	$4, %esp

  call isr_handler

  popl	%ds

  popal
  add $8, %esp

  movl %ebp, %esp
  popl %ebp

  iret

syscall_return_value:
  .long	0
.global _syscall_entry
_syscall_entry:
    cli
    addl $1, __cli_cnt

    pushl %ebp
    movl %esp, %ebp

    pushfl
    pushal

    pushl	%ds

	  pushl	%edx	/* Arg 3 */
    pushl	%ecx	/* Arg 2 */
    pushl	%ebx	/* Arg 1 */
    pushl	%eax	/* Syscall number */

    pushl	$16
    call	load_data_segments
    addl	$4, %esp

    call system_call
    movl	%eax, (syscall_return_value)
	
    popl	%edx	/* Arg 3 */
    popl	%ecx	/* Arg 2 */
    popl	%ebx	/* Arg 1 */
    popl	%eax	/* Syscall number */
    
    popl	%ds

    popal 
    popfl
    
    movl	(syscall_return_value), %eax

    movl %ebp, %esp
    popl %ebp

    subl $1, __cli_cnt
    iret

.section .note.GNU-stack,"",@progbits
