/**
 * @file kclock.c
 * @author Joe Bayer (joexbayer)
 * @brief Clock for kernel.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <kthreads.h>
#include <libc.h>
#include <colors.h>
#include <gfx/gfxlib.h>
#include <gfx/theme.h>
#include <gfx/window.h>
#include <gfx/events.h>
#include <timer.h>
#include <terminal.h>
#include <kutils.h>
#include <scheduler.h>
#include <math.h>
#include <timefmt.h>

#define center_x(size) ((110/2) - (((size)*8)/2))

void __kthread_entry kclock(int argc, char* argv[])
{   
    if(argc != 0){
        for (int i = 0; i < argc; i++){
            dbgprintf("kclock: argv[%d] = %s\n", i, argv[i]);
        }
        
    }

    int angle_id;
    struct time now;
    char text[TIME_FORMAT_DATE_MAX + 1];
    struct gfx_theme* theme;

    /* Second of the last redraw, -1 to draw right away */
    int last_second = -1;
    
    struct window* w = gfx_new_window(110, 140, 0);
    if(w == NULL){
        warningf("Failed to create window for kclock");
        return;
    }

    kernel_gfx_set_title("Clock");
    w->ops->move(w, 450, 50);
    while (1){
        /* Handle every pending event on each pass, so closing doesn't wait for the next redraw */
        struct gfx_event event;
        while(gfx_event_loop(&event, GFX_EVENT_NONBLOCKING) != -1){
            if(event.event == GFX_EVENT_EXIT){
                kernel_exit();
            }
        }

        get_current_time(&now);

        /**
         * Redraw each time the second changes, so the second hand ticks.
         * Unlike a difference of seconds since midnight, this keeps working past midnight.
         */
        if(now.second == last_second){
            /* Check again in 50 ms rather than spinning. Events don't wake a sleeping thread, so closing can take that long. */
            kernel_sleep(50);
            continue;
        }
        last_second = now.second;

        theme = kernel_gfx_current_theme();

        angle_id = (0.5 * (now.hour%12 * 60 + now.minute) / 6);

        w->draw->rect(w, 0, 0, 110, 140, 30);

        w->draw->circle(w, 55, 55, 50, COLOR_VGA_LIGHTEST_GRAY, 1);
        w->draw->circle(w, 55, 55, 50, 0, 0);

        /* draw small lines between numbers */
        for(int i = 0; i < 60; i++){
            if(i % 5 == 0) continue;
            w->draw->line(w, 55 + (50*sin_60[i])/1.1, 55+ (50*cos_60[i])/1.1, 55 + (50*sin_60[i])/1.2, 55+ (50*cos_60[i])/1.2, theme->window.text);
        }


        /* 12, 3, 6, 9 numbers */
        w->draw->text(w, center_x(2), 18,  "12", theme->window.text);
        w->draw->text(w, center_x(1), 100-4-8, "6", theme->window.text);
        w->draw->text(w, 18, 55, "9", theme->window.text);
        w->draw->text(w, 100-4-8, 55, "3", theme->window.text);


		w->draw->line(w, 55, 55, 55 + (50*sin_60[angle_id])/1.5, 55+ (50*cos_60[angle_id])/1.5, theme->window.text);
		w->draw->line(w, 55, 55, 55 + (50*sin_60[now.minute])/1.1, 55+ (50*cos_60[now.minute])/1.1, theme->window.text);
		w->draw->line(w, 55, 55, 55 + (50*sin_60[now.second])/1.1, 55+ (50*cos_60[now.second])/1.1, COLOR_VGA_RED);

        /* e.g. "2:05 PM" and "Oct 18", or "14:05" and "18 Oct", centered by their length */
        time_format_time(text, sizeof(text), &now, false);
        w->draw->text(w, center_x(strlen(text)), 112, text, theme->window.text);
        time_format_date(text, sizeof(text), &now, TIME_DATE_SHORT);
        w->draw->text(w, center_x(strlen(text)), 124, text, theme->window.text);

        /* Mark the window changed, so the screen is redrawn even when nothing else changes */
        gfx_commit();
    }
}
EXPORT_KTHREAD(kclock);