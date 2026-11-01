/**
 * @file api.c
 * @author Joe Bayer (joexbayer)
 * @brief Graphics API from userspace
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <gfx/api.h>
#include <gfx/gfxlib.h>
#include <lib/graphics.h>
#include <colors.h>
#include <gfx/events.h>
#include <assert.h>
#include <errors.h>
#include <memory.h>

int gfx_syscall_hook(int option, void* data, int flags)
{
    if(option >= GFX_DRAW_UNUSED || option < 0) return -1;
    if(!(flags >= 0 && flags < 255))return -1;

    ERR_ON_NULL(data);
    ERR_ON_NULL($process->current);

    /**
     * An app's frame is everything it draws between event reads: reading the next one ends it, and waits until the
     * window server has shown it (gfx_present). So the window server never shows a half-drawn frame, and the draw
     * calls themselves don't wake it.
     */
    if(option == GFX_EVEN_LOOP_OPT){
        gfx_present();
        return gfx_event_loop((struct gfx_event*)data, flags);
    }

    struct window* w = $process->current->gfx_window;
    if(w != NULL){
        gfx_frame_begin(w);
    }

    switch (option){
    case GFX_DRAW_CHAR_OPT:;
        struct gfx_char* c = (struct gfx_char*)data;
        kernel_gfx_draw_char($process->current->gfx_window, c->x, c->y, c->data, c->color);
        break;
    case GFX_DRAW_RECTANGLE_OPT:;
        struct gfx_rectangle* r = (struct gfx_rectangle*)data;
        kernel_gfx_draw_rectangle($process->current->gfx_window, r->x, r->y, r->width, r->height, r->color);
        break;
    
    case GFX_DRAW_CIRCLE_OPT:;
        struct gfx_circle* circle = (struct gfx_circle*)data;
        kernel_gfx_draw_circle($process->current->gfx_window, circle->x, circle->y, circle->r, circle->color, circle->fill);
        break;
    
    case GFX_DRAW_LINE_OPT:;
        struct gfx_line* line = (struct gfx_line*)data;
        kernel_gfx_draw_line($process->current->gfx_window, line->x0, line->y0, line->x1, line->y1, line->color);
        break;
    
    case GFX_DRAW_PIXEL:;
        struct gfx_pixel* pixel = (struct gfx_pixel*)data;
        kernel_gfx_draw_pixel($process->current->gfx_window, pixel->x, pixel->y, pixel->color);
        break;

    case GFX_DRAW_IMAGE:;
        /* The arguments, then every pixel, are in the app's memory, which is checked before anything is read */
        struct gfx_image* draw = (struct gfx_image*)data;
        if(user_memory_validate(draw, sizeof(*draw), 0) != ERROR_OK){
            return -ERROR_INVALID_ARGUMENTS;
        }
        struct gfx_image args = *draw;
        if(args.image.width < 1 || args.image.width > GFX_IMAGE_MAX_SIDE || args.image.height < 1 ||
            args.image.height > GFX_IMAGE_MAX_SIDE ||
            user_memory_validate(args.image.pixels, args.image.width * args.image.height, 0) != ERROR_OK){
            return -ERROR_INVALID_ARGUMENTS;
        }
        kernel_gfx_draw_image($process->current->gfx_window, args.x, args.y, &args.image);
        break;

    default:
        break;
    }

    /* Changed, but drawing until the frame ends, unless it takes longer than WS_FRAME_MAX_MS */
    if(w != NULL){
        w->changed = 1;
    }
    return ERROR_OK;
}