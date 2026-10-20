#ifndef TIMEFMT_H
#define TIMEFMT_H

#include <rtc.h>
#include <kutils.h>

/**
 * Formats a struct time as text, following the [time] section of sysutil/default.cfg:
 *   clock=12h or 24h
 *   date=mdy, dmy or ymd
 * Missing or unknown values mean 12h and mdy.
 */

enum time_date_style {
    TIME_DATE_NUMERIC,  /* 10/18/2026, 18/10/2026 or 2026-10-18 */
    TIME_DATE_LONG,     /* Oct 18, 2026, 18 Oct 2026 or 2026-10-18 */
    TIME_DATE_SHORT     /* Oct 18, 18 Oct or 10-18 */
};

/* The longest text each call returns, not counting the terminating 0. Size buffers and screen space with these. */
#define TIME_FORMAT_TIME_MAX            8   /* "12:00 PM" */
#define TIME_FORMAT_TIME_SECONDS_MAX    11  /* "12:00:00 PM" */
#define TIME_FORMAT_DATE_MAX            12  /* "Sep 30, 2026", the longest style */
#define TIME_FORMAT_NUMERIC_DATE_MAX    10  /* "10/18/2026" or "2026-10-18" */

/**
 * Both write at most size-1 characters and a terminating 0, cutting off what doesn't fit,
 * and return the number of characters written.
 */
int time_format_time(char* buf, int size, const struct time* t, bool_t seconds);
int time_format_date(char* buf, int size, const struct time* t, enum time_date_style style);

#endif /* TIMEFMT_H */
