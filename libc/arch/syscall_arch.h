/**
 * RetrOS: replaces musl's arch/i386/syscall_arch.h, by coming first in the include path.
 *
 * RetrOS doesn't have Linux's system calls. musl's own code makes a few, like stdio's read, writev and open, and
 * __retros_linux_syscall (src/linux.c) does those with the library's RetrOS functions. Any other gets ENOSYS, rather
 * than an interrupt the kernel doesn't handle.
 */
#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

hidden long __retros_linux_syscall(long n, long a1, long a2, long a3, long a4, long a5, long a6);

static inline long __syscall0(long n)
{
	return __retros_linux_syscall(n, 0, 0, 0, 0, 0, 0);
}

static inline long __syscall1(long n, long a1)
{
	return __retros_linux_syscall(n, a1, 0, 0, 0, 0, 0);
}

static inline long __syscall2(long n, long a1, long a2)
{
	return __retros_linux_syscall(n, a1, a2, 0, 0, 0, 0);
}

static inline long __syscall3(long n, long a1, long a2, long a3)
{
	return __retros_linux_syscall(n, a1, a2, a3, 0, 0, 0);
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	return __retros_linux_syscall(n, a1, a2, a3, a4, 0, 0);
}

static inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	return __retros_linux_syscall(n, a1, a2, a3, a4, a5, 0);
}

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	return __retros_linux_syscall(n, a1, a2, a3, a4, a5, a6);
}
