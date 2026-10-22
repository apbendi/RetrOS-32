/**
 * RetrOS's side of the C library: its system calls, and its error codes as errno values.
 *
 * The kernel's own headers give the numbers: syscall_helper.h, errors.h and fs/fs_types.h, which can be included next
 * to standard headers. The Makefile puts the kernel's include directory after musl's, so nothing of the kernel's
 * shadows a standard header.
 */
#ifndef RETROS_LIBC_H
#define RETROS_LIBC_H

#include <time.h>
#include <syscall_helper.h>
#include <errors.h>
#include <fs/fs_types.h>

/* A RetrOS system call: int $0x30, with the call's number in eax and up to three arguments in ebx, ecx and edx */
static inline int __retros_syscall(int n, int a, int b, int c)
{
	int ret;
	__asm__ __volatile__ ("int $48" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return ret;
}

/* The errno value for one of RetrOS's error codes, which system calls return negated */
int __retros_errno(int error);

/* A system call's result as POSIX functions return it: itself if it isn't negative, otherwise -1, setting errno */
int __retros_ret(int ret);

/* The time a struct time from the kernel describes, in seconds since 1970. The clock holds local time. */
time_t __retros_time(const struct time* t);

#endif
