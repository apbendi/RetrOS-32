#ifndef RTC_H
#define RTC_H

#include <stdint.h>

struct time {
    uint8_t centuary;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

int get_current_time(struct time* time);
int get_time();
const char* rtc_month_name(int month);

#endif /* RTC_H */
