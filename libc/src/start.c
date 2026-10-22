/**
 * A program's start, after crt1.o: the stand-in thread structure musl needs (arch/pthread_arch.h), the environment,
 * constructors, then main() and exit().
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pthread_impl.h"
#include "libc.h"

/* Every thread of the program shares it: RetrOS has no thread pointer for musl */
struct pthread __retros_thread;

/* RetrOS has no environment variables yet */
static char* empty_environment[] = { 0 };

extern char** __environ;
extern char *__progname, *__progname_full;
extern int main(int, char**, char**);

/* Constructors, which crti.o and crtn.o don't run: they're empty */
extern weak hidden void (*const __init_array_start)(void), (*const __init_array_end)(void);

static void __init_thread(void)
{
	struct pthread* self = &__retros_thread;
	self->self = self;
	self->tid = 1;
	self->detach_state = DT_JOINABLE;
	self->locale = &libc.global_locale;
	self->robust_list.head = &self->robust_list.head;
	self->next = self->prev = self;
}

_Noreturn void __retros_start_main(int argc, char** argv)
{
	__init_thread();
	__environ = empty_environment;
	if(argc > 0 && argv[0]){
		char* slash = strrchr(argv[0], '/');
		__progname_full = argv[0];
		__progname = slash ? slash + 1 : argv[0];
	}

	for(void (*const* f)(void) = &__init_array_start; f < &__init_array_end; f++){
		(*f)();
	}

	exit(main(argc, argv, __environ));
}

/* A thread's function has returned (crt1.o). Only the thread ends; exit() would end the program. */
_Noreturn void __retros_thread_exit(void)
{
	_exit(0);
}
