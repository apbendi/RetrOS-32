/**
 * The clock and sleeping. RetrOS's real-time clock holds local time, and RetrOS has no time zones, so time() is local
 * time counted as if it were UTC, and localtime() (musl's, with no TZ) gives it back unchanged. The clock has whole
 * seconds. CLOCK_MONOTONIC is the timer's milliseconds since boot instead, for measuring how long something takes.
 */
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include "retros.h"

time_t time(time_t* out)
{
	struct time now;
	__retros_syscall(SYSCALL_GFX_GET_TIME, (int)&now, 0, 0);

	time_t t = __retros_time(&now);
	if(out) *out = t;
	return t;
}

int clock_gettime(clockid_t clock, struct timespec* ts)
{
	switch(clock){
	case CLOCK_REALTIME:
		ts->tv_sec = time(0);
		ts->tv_nsec = 0;
		return 0;
	case CLOCK_MONOTONIC: {
		unsigned ms = (unsigned)__retros_syscall(SYSCALL_GET_MS, 0, 0, 0);
		ts->tv_sec = ms / 1000;
		ts->tv_nsec = (long)(ms % 1000) * 1000000;
		return 0;
	}
	default:
		errno = EINVAL;
		return -1;
	}
}

int gettimeofday(struct timeval* restrict tv, void* restrict tz)
{
	(void)tz;
	if(tv){
		tv->tv_sec = time(0);
		tv->tv_usec = 0;
	}
	return 0;
}

/* RetrOS doesn't count a program's processor time, so clock() can't tell */
clock_t clock(void)
{
	return (clock_t)-1;
}

/* RetrOS's sleep system call takes milliseconds */
static void __sleep_ms(unsigned long ms)
{
	while(ms > 0){
		unsigned long part = ms > 1000000 ? 1000000 : ms;
		__retros_syscall(SYSCALL_SLEEP, (int)part, 0, 0);
		ms -= part;
	}
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
	if(req->tv_nsec < 0 || req->tv_nsec >= 1000000000L || req->tv_sec < 0){
		errno = EINVAL;
		return -1;
	}
	__sleep_ms((unsigned long)req->tv_sec * 1000 + (req->tv_nsec + 999999) / 1000000);
	if(rem) rem->tv_sec = rem->tv_nsec = 0;
	return 0;
}

unsigned sleep(unsigned seconds)
{
	__sleep_ms((unsigned long)seconds * 1000);
	return 0;
}

int usleep(useconds_t us)
{
	__sleep_ms((us + 999) / 1000);
	return 0;
}
