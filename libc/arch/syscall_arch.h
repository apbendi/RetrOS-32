/**
 * RetrOS: replaces musl's arch/i386/syscall_arch.h, by coming first in the include path.
 *
 * RetrOS doesn't have Linux's system calls. The parts of musl that RetrOS builds have RetrOS versions of whatever
 * makes system calls (src/), so nothing should get here. If something does, it gets ENOSYS, rather than an interrupt
 * the kernel doesn't handle.
 */
#define __SYSCALL_LL_E(x) \
((union { long long ll; long l[2]; }){ .ll = x }).l[0], \
((union { long long ll; long l[2]; }){ .ll = x }).l[1]
#define __SYSCALL_LL_O(x) __SYSCALL_LL_E((x))

static inline long __syscall0(long n)
{
	(void)n;
	return -ENOSYS;
}

static inline long __syscall1(long n, long a1)
{
	(void)a1;
	return __syscall0(n);
}

static inline long __syscall2(long n, long a1, long a2)
{
	(void)a2;
	return __syscall1(n, a1);
}

static inline long __syscall3(long n, long a1, long a2, long a3)
{
	(void)a3;
	return __syscall2(n, a1, a2);
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	(void)a4;
	return __syscall3(n, a1, a2, a3);
}

static inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	(void)a5;
	return __syscall4(n, a1, a2, a3, a4);
}

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	(void)a6;
	return __syscall5(n, a1, a2, a3, a4, a5);
}
