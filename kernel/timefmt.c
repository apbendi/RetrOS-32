/**
 * @file timefmt.c
 * @brief Formats times and dates for display, following the [time] settings in sysutil/default.cfg.
 *
 * The kernel's printf has no field widths such as %02d, so the padding is done here.
 */

#include <timefmt.h>
#include <conf.h>

enum time_clock {
    TIME_CLOCK_12H,
    TIME_CLOCK_24H
};

enum time_date_order {
    TIME_DATE_MDY,
    TIME_DATE_DMY,
    TIME_DATE_YMD
};

/* The settings are read on every call, so "conf load" takes effect right away. It's a few string compares. */
static enum time_clock __time_clock(void)
{
    return kernel_config_check("time", "clock", "24h") ? TIME_CLOCK_24H : TIME_CLOCK_12H;
}

static enum time_date_order __time_date_order(void)
{
    if(kernel_config_check("time", "date", "dmy")) return TIME_DATE_DMY;
    if(kernel_config_check("time", "date", "ymd")) return TIME_DATE_YMD;
    return TIME_DATE_MDY;
}

/* Text being built in a buffer of a fixed size. It's always terminated, and what doesn't fit is dropped. */
struct text {
    char* buf;
    int size;
    int length;
};

static void __text_init(struct text* text, char* buf, int size)
{
    text->buf = buf;
    text->size = size;
    text->length = 0;
    if(size > 0){
        buf[0] = '\0';
    }
}

static void __text_char(struct text* text, char c)
{
    if(text->length >= text->size - 1){
        return;
    }
    text->buf[text->length++] = c;
    text->buf[text->length] = '\0';
}

static void __text_string(struct text* text, const char* str)
{
    while(*str != '\0'){
        __text_char(text, *str++);
    }
}

/* Writes a non-negative number with at least min_digits digits, padding with zeros */
static void __text_number(struct text* text, int number, int min_digits)
{
    char digits[10];
    int count = 0;

    if(number < 0){
        number = 0;
    }
    do {
        digits[count++] = '0' + number % 10;
        number /= 10;
    } while(number > 0 && count < (int)sizeof(digits));

    for(int i = count; i < min_digits; i++){
        __text_char(text, '0');
    }
    while(count > 0){
        __text_char(text, digits[--count]);
    }
}

/**
 * 12-hour: "2:05 PM", or "2:05:09 PM" with seconds. Hour 0 is 12 AM, and hour 12 is 12 PM.
 * 24-hour: "14:05", or "14:05:09" with seconds.
 */
int time_format_time(char* buf, int size, const struct time* t, bool_t seconds)
{
    struct text text;
    __text_init(&text, buf, size);

    if(__time_clock() == TIME_CLOCK_24H){
        __text_number(&text, t->hour, 2);
    } else {
        __text_number(&text, t->hour % 12 == 0 ? 12 : t->hour % 12, 1);
    }

    __text_char(&text, ':');
    __text_number(&text, t->minute, 2);
    if(seconds){
        __text_char(&text, ':');
        __text_number(&text, t->second, 2);
    }

    if(__time_clock() == TIME_CLOCK_12H){
        __text_string(&text, t->hour < 12 ? " AM" : " PM");
    }

    return text.length;
}

/* See enum time_date_style for what each style looks like in each order */
int time_format_date(char* buf, int size, const struct time* t, enum time_date_style style)
{
    struct text text;
    int year = t->centuary * 100 + t->year;
    enum time_date_order order = __time_date_order();

    __text_init(&text, buf, size);

    /* Year first always uses numbers, as in ISO 8601 */
    if(order == TIME_DATE_YMD){
        if(style != TIME_DATE_SHORT){
            __text_number(&text, year, 4);
            __text_char(&text, '-');
        }
        __text_number(&text, t->month, 2);
        __text_char(&text, '-');
        __text_number(&text, t->day, 2);
        return text.length;
    }

    if(style == TIME_DATE_NUMERIC){
        __text_number(&text, order == TIME_DATE_MDY ? t->month : t->day, 2);
        __text_char(&text, '/');
        __text_number(&text, order == TIME_DATE_MDY ? t->day : t->month, 2);
        __text_char(&text, '/');
        __text_number(&text, year, 4);
        return text.length;
    }

    if(order == TIME_DATE_MDY){
        __text_string(&text, rtc_month_name(t->month));
        __text_char(&text, ' ');
        __text_number(&text, t->day, 1);
        if(style == TIME_DATE_LONG){
            __text_string(&text, ", ");
            __text_number(&text, year, 4);
        }
    } else {
        __text_number(&text, t->day, 1);
        __text_char(&text, ' ');
        __text_string(&text, rtc_month_name(t->month));
        if(style == TIME_DATE_LONG){
            __text_char(&text, ' ');
            __text_number(&text, year, 4);
        }
    }

    return text.length;
}
