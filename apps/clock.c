/**
 * @file clock.c
 * @author Joe Bayer (joexbayer)
 * @brief A simple clock program displaying
 * current time and date.
 * @version 0.1
 * @date 2023-02-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <lib/printf.h>
#include <lib/graphics.h>
#include <gfx/events.h>
#include <colors.h>
#include <rtc.h>
#include <libc.h>
#include <syscall_helper.h>

/* The window's content: as wide as the narrowest a window can be (GFX_WINDOW_MIN_WIDTH, less the frame) */
#define CLOCK_WIDTH 112
#define CLOCK_HEIGHT 140
/* The face's center */
#define FACE_X (CLOCK_WIDTH/2)
#define FACE_Y 55

#define center_x(size) ((CLOCK_WIDTH/2) - ((size*8)/2))

/* Apps can't call the kernel's rtc_month_name, so this is its own copy */
static char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static volatile int shared;

int main()
{   
    struct time now;
    struct gfx_event event;
    gfx_create_window(CLOCK_WIDTH, CLOCK_HEIGHT, 0);
    gfx_set_title("Time & Date");

    printf("Clock started\n");
    while (1){
        get_current_time(&now);

        gfx_draw_rectangle(0, 0, CLOCK_WIDTH, CLOCK_HEIGHT, 30);

        int angle_id = (0.5 * (now.hour%12 * 60 + now.minute) / 6);
        gfx_draw_circle(FACE_X, FACE_Y, 50, 0xf, 1);
        gfx_draw_circle(FACE_X, FACE_Y, 50, 0, 0);

		gfx_draw_line(FACE_X, FACE_Y, FACE_X + (50*cos_60[angle_id])/1.5, FACE_Y + (50*sin_60[angle_id])/1.5, COLOR_VGA_MEDIUM_GRAY);
		gfx_draw_line(FACE_X, FACE_Y, FACE_X + (50*cos_60[now.minute])/1.1, FACE_Y + (50*sin_60[now.minute])/1.1, COLOR_VGA_MEDIUM_GRAY);
		gfx_draw_line(FACE_X, FACE_Y, FACE_X + (50*cos_60[now.second])/1.1, FACE_Y + (50*sin_60[now.second])/1.1, COLOR_VGA_RED);

        gfx_draw_format_text(center_x(5), 112, COLOR_VGA_MEDIUM_GRAY, "%s%d:%s%d", now.hour > 9 ? "" : "0", now.hour, now.minute > 9 ? "" : "0", now.minute);
        gfx_draw_format_text(center_x(6), 124, COLOR_VGA_MEDIUM_GRAY, "%d. %s", now.day, now.month >= 1 && now.month <= 12 ? months[now.month - 1] : "???");

        gfx_draw_format_text(0, 0, COLOR_WHITE, "%d", shared);
        sleep(100);

        int ret = gfx_get_event(&event, GFX_EVENT_NONBLOCKING);
        if(ret == -1) continue;

        switch (event.event){
        case GFX_EVENT_EXIT:
            exit();
            break;
        default:
            break;
        }

    }
	return 0;
}