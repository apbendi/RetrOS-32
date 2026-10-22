/* musl's freopen() keeps the stream's descriptor number with __dup3(). RetrOS can't duplicate a descriptor, so
 * freopen() with a new path fails cleanly, with ENOSYS. */
#include <errno.h>

int __dup3(int old, int new, int flags);

int __dup3(int old, int new, int flags)
{
	(void)old;
	(void)new;
	(void)flags;
	errno = ENOSYS;
	return -1;
}
