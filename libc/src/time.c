/* time(), from RetrOS's real-time clock, which holds local time: RetrOS has no time zones */
#include <time.h>
#include "retros.h"

time_t time(time_t* out)
{
	struct time now;
	__retros_syscall(SYSCALL_GFX_GET_TIME, (int)&now, 0, 0);

	time_t t = __retros_time(&now);
	if(out) *out = t;
	return t;
}
