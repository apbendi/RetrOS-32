/**
 * @file rtc.c
 * @author Joe Bayer (joexbayer)
 * @brief RTC "driver" for getting currentime and date.
 * @see https://wiki.osdev.org/CMOS
 * @version 0.1
 * @date 2022-06-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <rtc.h>
#include <arch/io.h>
#include <kutils.h>

enum {
    RTC_ADDRESS = 0x70,
    RTC_DATA    = 0x71
};

int __rtc_check_in_progress()
{
      outportb(RTC_ADDRESS, 0x0A);
      return (inportb(RTC_DATA) & 0x80);
}

uint8_t __rtc_get_register(int reg)
{
    outportb(RTC_ADDRESS, reg);
    return inportb(RTC_DATA);
}


/**
 * Returns exactly what the RTC holds, with no time zone handling. The Makefile starts QEMU with
 * -rtc base=localtime, so that's the host's local time. #20 covers keeping it in UTC with an offset setting.
 */
int get_current_time(struct time* time)
{
    uint8_t reg_b;

    /**
     * Selecting a register and reading it are separate port accesses, so another thread reading the RTC
     * in between would change which register gets read. Interrupts stay off from the update check through
     * the last read, so no other thread runs, and the reads finish well within the 244 microseconds the RTC
     * guarantees after the update-in-progress bit clears. An update takes up to 2 ms, so interrupts come
     * back on between checks.
     */
    while (1) {
        ENTER_CRITICAL();
        if (!__rtc_check_in_progress()) {
            break;
        }
        LEAVE_CRITICAL();
    }
    time->second = __rtc_get_register(0x00);
    time->minute = __rtc_get_register(0x02); 
    time->hour = __rtc_get_register(0x04); 
    time->day = __rtc_get_register(0x07); 
    time->month = __rtc_get_register(0x08); 
    time->year = __rtc_get_register(0x09); 

    reg_b = __rtc_get_register(0x0B);
    LEAVE_CRITICAL();

    if (!(reg_b & 0x04))
    {
        time->second = (time->second & 0x0F) + ((time->second / 16) * 10);
        time->minute = (time->minute & 0x0F) + ((time->minute / 16) * 10);
        time->hour = (( (time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10) ) | (time->hour & 0x80));
        time->day = (time->day & 0x0F) + ((time->day / 16) * 10);
        time->month = (time->month & 0x0F) + ((time->month / 16) * 10);
        time->year = (time->year & 0x0F) + ((time->year / 16) * 10);
    }

    /* 12-hour mode stores 1-12 with bit 7 set for PM, so midnight is 12 and noon is 12 | 0x80 */
    if (!(reg_b & 0x02)) {
        time->hour = (time->hour & 0x7F) % 12 + ((time->hour & 0x80) ? 12 : 0);
    }

    return 1;
}

int get_time()
{
    struct time t;
    get_current_time(&t);
	return (t.hour*3600)+(t.minute*60)+t.second;
}

/**
 * Returns the three-letter name of a month from 1 to 12, or "???" for anything else.
 */
const char* rtc_month_name(int month)
{
    static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (month < 1 || month > 12) {
        return "???";
    }
    return names[month - 1];
}
