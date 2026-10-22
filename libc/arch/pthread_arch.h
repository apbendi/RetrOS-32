/**
 * RetrOS: replaces musl's arch/i386/pthread_arch.h, by coming first in the include path.
 *
 * musl finds the current thread's structure through a thread pointer, which on i386 is %gs:0. RetrOS doesn't set up
 * %gs per thread, so the "current thread" is one static structure, set up at startup (src/start.c). Every thread of a
 * program shares it, and with it errno and the locale.
 */
extern struct __pthread __retros_thread;

static inline uintptr_t __get_tp()
{
	return (uintptr_t)&__retros_thread;
}

#define MC_PC gregs[REG_EIP]
