/* RetrOS's error codes as errno values, and its clock's time as a time_t (retros.h) */
#include <errno.h>
#include <time.h>
#include "retros.h"

int __retros_errno(int error)
{
	switch(-error){
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return ENOENT;
	case ERROR_FILE_EXISTS:
		return EEXIST;
	case ERROR_IS_DIRECTORY:
		return EISDIR;
	case ERROR_NOT_DIRECTORY:
		return ENOTDIR;
	case ERROR_DIRECTORY_NOT_EMPTY:
		return ENOTEMPTY;
	case ERROR_NO_SPACE:
		return ENOSPC;
	case ERROR_ACCESS_DENIED:
		return EACCES;
	case ERROR_ALLOC:
	case ERROR_OUT_OF_MEMORY:
		return ENOMEM;
	case ERROR_FILE_TOO_LARGE:
		return EFBIG;
	case ERROR_NULL_POINTER:
		return EFAULT;
	case ERROR_NO_FILESYSTEM:
		return ENODEV;
	case ERROR_INVALID_NAME:
	case ERROR_INVALID_ARGUMENTS:
	case ERROR_MOVE_INTO_ITSELF:
		return EINVAL;
	case ERROR_DISK:
	default:
		return EIO;
	}
}

int __retros_ret(int ret)
{
	if(ret >= 0) return ret;
	errno = __retros_errno(ret);
	return -1;
}

/* Days from 1970-01-01 to a date in the proleptic Gregorian calendar, by Howard Hinnant's days_from_civil */
static long __days_from_civil(int y, int m, int d)
{
	y -= m <= 2;
	int era = (y >= 0 ? y : y - 399) / 400;
	int yoe = y - era * 400;
	int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (long)era * 146097 + doe - 719468;
}

time_t __retros_time(const struct time* t)
{
	/* A centuary of 0 means there's no date, e.g. a file written without a clock */
	if(t->centuary == 0) return 0;

	int year = t->centuary * 100 + t->year;
	long days = __days_from_civil(year, t->month, t->day);
	return (time_t)days * 86400 + t->hour * 3600 + t->minute * 60 + t->second;
}
