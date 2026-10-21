/**
 * @file window.c
 * @author Joe Bayer (joexbayer)
 * @brief GFX Window API
 * @version 0.1
 * @date 2023-01-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <gfx/window.h>
#include <libc.h>
#include <gfx/component.h>
#include <gfx/composition.h>
#include <gfx/theme.h>
#include <gfx/gfxlib.h>
#include <colors.h>
#include <serial.h>
#include <vbe.h>
#include <errors.h>
#include <kutils.h>
#include <lib/icons.h>
#include <math.h>

//#define __WINDOWS_95


/* prototype window ops */
static void gfx_default_click(struct window* window, int x, int y); 
static void gfx_default_mouse_down(struct window* window, int x, int y);
static void gfx_default_mouse_up(struct window* window, int x, int y);
static void gfx_default_hover(struct window* window, int x, int y);
static int gfx_window_resize(struct window* w, int width, int height);
static int gfx_window_maximize(struct window* window);

/* default window ops struct */
static struct window_ops default_window_ops = {
    .click = &gfx_default_click,
    .mousedown = &gfx_default_mouse_down,
    .mouseup = &gfx_default_mouse_up,
    .hover = &gfx_default_hover,
    .resize = &gfx_window_resize,
    .move = &kernel_gfx_set_position,
    .destroy = &gfx_destroy_window,
    .maximize = &gfx_window_maximize
};

/* default windows draw ops */
static struct window_draw_ops default_window_draw_ops = {
    .draw = &gfx_draw_window,
    .rect = &kernel_gfx_draw_rectangle,
    .textf = &kernel_gfx_draw_format_text,
    .text = &kernel_gfx_draw_text,
    .line = &kernel_gfx_draw_line,
    .circle = &kernel_gfx_draw_circle,
    .box = &kernel_gfx_draw_contoured_box,
    .bitmap = &kernel_gfx_draw_bitmap
};

/**
 * @brief Draw given window to a frambuffer.
 * 
 * @param buffer 
 * @param window 
 */
void gfx_draw_window(uint8_t* buffer, struct window* window)
{
    /**
     * Clear changed before copying the window, not after. Its owner may commit while the copy is under way,
     * and clearing it afterwards would drop that commit, leaving a half-drawn frame on screen until something else changes.
     */
    window->changed = 0;

    struct gfx_theme* theme = kernel_gfx_current_theme();
    int padding = HAS_FLAG(window->flags, GFX_HIDE_HEADER) ? 0 : 8;
    int background_color = window->in_focus ? window->color.border == 0 ? theme->window.border : window->color.border : theme->window.border;

    color_t border_light = window->color.border == 0 ? theme->window.border_accent_light : window->color.border;
    color_t border_dark = window->color.border == 0 ? theme->window.border_accent_dark : window->color.border;

    if((window->is_moving.state == GFX_WINDOW_MOVING || window->resize) && !HAS_FLAG(window->flags, GFX_IS_IMMUATABLE)){
        /* During a resize, the outline shows the size the window gets on release */
        int width = window->resize ? window->pending_size.width+16 : window->width;
        int height = window->resize ? window->pending_size.height+16 : window->height;

        vesa_striped_line_horizontal(buffer, window->x, window->y, width, COLOR_VGA_DARKEST_GRAY, 2);
        vesa_striped_line_horizontal(buffer, window->x+2, window->y, width-2, COLOR_VGA_LIGHTEST_GRAY, 4);
        
        /* Bottom and right edges go on the window's last row and column, not past them, so they stay on screen */
        vesa_striped_line_horizontal(buffer, window->x, window->y+height-1, width, COLOR_VGA_DARKEST_GRAY, 2);
        vesa_striped_line_horizontal(buffer, window->x+2, window->y+height-1, width-2, COLOR_VGA_LIGHTEST_GRAY, 4);

        vesa_striped_line_vertical(buffer, window->x, window->y, height, COLOR_VGA_DARKEST_GRAY, 2);
        vesa_striped_line_vertical(buffer, window->x, window->y+2, height-2, COLOR_VGA_LIGHTEST_GRAY, 4);

        vesa_striped_line_vertical(buffer, window->x+width-1, window->y, height, COLOR_VGA_DARKEST_GRAY, 2);
        vesa_striped_line_vertical(buffer, window->x+width-1, window->y+2, height-2, COLOR_VGA_LIGHTEST_GRAY, 4);

        return;
    }

    /**
     * Copy the window's content into the screen buffer, if it isn't minimized. Only the part on screen is copied,
     * so a window partly off screen is cut off instead of writing past the buffer (#4). The source is indexed by
     * the window's own row and column, so the visible part stays aligned.
     */
    if(window->inner != NULL && !HAS_FLAG(window->flags, GFX_IS_HIDDEN)){
        int left = window->x+padding;
        int top = window->y+padding;
        int x_start = vesa_clip_start(left), x_end = vesa_clip_end(left+window->inner_width, vbe_info->width);
        int y_start = vesa_clip_start(top), y_end = vesa_clip_end(top+window->inner_height, vbe_info->height);
        bool_t transparent = HAS_FLAG(window->flags, GFX_IS_TRANSPARENT);

        for (int j = y_start; j < y_end; j++){
            uint8_t* source = window->inner + (j-top)*window->pitch + (x_start-left);
            for (int i = x_start; i < x_end; i++, source++){
                if(transparent && *source == 255){
                    continue;
                }
                putpixel(buffer, i, j, *source, vbe_info->pitch);
            }
        }
    }

     if(!HAS_FLAG(window->flags, GFX_HIDE_HEADER)){            
        /* Draw main frame of window with title bar and borders */
#ifdef __WINDOWS_95
        color_t header_color = window->in_focus ? 0x1 : COLOR_VGA_DARK_GRAY;
        color_t header_text_color = window->in_focus ? 0x0f : COLOR_VGA_LIGHT_GRAY;
#else 
        color_t header_color = theme->window.background;
        color_t header_text_color = window->in_focus ? 0x00 : COLOR_VGA_LIGHT_GRAY;
#endif // __WINDOWS_95

        vesa_fillrect(buffer, window->x+6, window->y-4, window->width-10, 12, header_color);

        /* top */
        vesa_line_horizontal(buffer, window->x+4, window->y-4, window->width-8, border_dark);
        vesa_line_horizontal(buffer, window->x+4, window->y-3, window->width-8, border_light);

        /* left */
        vesa_line_horizontal(buffer, window->x+4, window->y+8, window->width-8, border_dark);
        vesa_line_horizontal(buffer, window->x+4, window->y+7, window->width-8, background_color);

        if(window->in_focus){
#ifndef __WINDOWS_95
            for (int i = 0; i < 3; i++){
                vesa_line_horizontal(buffer, window->x+8, (window->y-1)+i*3, window->width-16, background_color);
                vesa_line_horizontal(buffer, window->x+8, (window->y-2)+i*3, window->width-16, border_light);
            }
#endif // __WINDOWS_95
        }

        /* Title */
        int title_position = (window->width/2) - ((strlen(window->name)*8)/2);
        vesa_fillrect(buffer, window->x+title_position, window->y-2, strlen(window->name)*8 + 4, 8, header_color);
        vesa_write_str(buffer, window->x+title_position+4, window->y-2, window->name, header_text_color);
    }

    if(!HAS_FLAG(window->flags, GFX_HIDE_BORDER) && !HAS_FLAG(window->flags, GFX_IS_HIDDEN)){
         /* bottom */
        vesa_fillrect(buffer, window->x+4, window->y+window->height-8, window->width-8, 4, theme->window.background);
        vesa_line_horizontal(buffer, window->x+4, window->y+window->height-4, window->width-8, border_dark);
        vesa_line_horizontal(buffer, window->x+4, window->y+window->height-5, window->width-8, background_color);

        vesa_line_horizontal(buffer, window->x+4, window->y+window->height-8, window->width-8, border_dark);
        vesa_line_horizontal(buffer, window->x+4, window->y+window->height-7, window->width-8, border_light);
        
        /* left */
        vesa_fillrect(buffer, window->x+4, window->y+8, 4, window->height-4-8, theme->window.background);
        vesa_line_vertical(buffer, window->x+4, window->y-3, window->height, border_dark);
        vesa_line_vertical(buffer, window->x+5, window->y-3, window->height-1, border_light);

        vesa_line_vertical(buffer, window->x+8, window->y+8, window->height-16, border_dark);
        vesa_line_vertical(buffer, window->x+7, window->y+8, window->height-16, background_color);

        /* right */
        vesa_fillrect(buffer, window->x+window->width-8, window->y+8, 4, window->height-4-8, theme->window.background);
        vesa_line_vertical(buffer, window->x+window->width-4, window->y-4, window->height, border_dark);
        vesa_line_vertical(buffer, window->x+window->width-5, window->y-4, window->height, background_color);

        vesa_line_vertical(buffer, window->x+window->width-8, window->y+8, window->height-16, border_dark);
        vesa_line_vertical(buffer, window->x+window->width-7, window->y+8, window->height-16, border_light);
    }

    if(!HAS_FLAG(window->flags, GFX_HIDE_HEADER) && !HAS_FLAG(window->flags, GFX_NO_OPTIONS)){

        /* full screen */
        if(window->is_resizable){    
            vesa_inner_box(buffer, window->x+window->width-46+6,  window->y-3, 10, 9, theme->window.background);
            vesa_write_str(buffer, window->x+window->width-44+6,  window->y-2, "^", COLOR_VGA_DARK_GRAY);
        }

        /* Minimize */
        vesa_inner_box(buffer, window->x+window->width-34+6,  window->y-3, 10, 9, theme->window.background);
        vesa_write_str(buffer, window->x+window->width-32+6,  window->y-2, "-", COLOR_VGA_DARK_GRAY);

        /* Exit */
        vesa_inner_box(buffer, window->x+window->width-22+6,  window->y-3, 10, 9, theme->window.background);
        vesa_write_str(buffer, window->x+window->width-20+6,  window->y-2, "X", COLOR_VGA_DARK_GRAY);
    }

    return;
}   

void gfx_window_set_resizable()
{
    $process->current->gfx_window->is_resizable = 1;
}

/**
 * @brief Limit a content size (16 px smaller than the window) to what a resize allows.
 * Keep the title bar usable, but staying on screen wins: drawing is clipped (#4), so a window past
 * the edge would just be cut off, with its resize corner out of reach.
 * Used by both the resize outline and the resize itself, so they agree.
 */
static void gfx_window_clamp_size(struct window* w, int* width, int* height)
{
    *width = MIN(MAX(*width, GFX_WINDOW_MIN_WIDTH-16), vbe_info->width - w->x - 16);
    *height = MIN(MAX(*height, GFX_WINDOW_MIN_HEIGHT-16), vbe_info->height - w->y - 16);
}

/* under construction */
static int gfx_window_resize(struct window* w, int width, int height)
{
    /**
     * A fullscreen window's buffer is the window server's composition buffer. Resizing would free it
     * while the window server still draws into it. F10 gives the window its own buffer back first.
     */
    if(w->is_fullscreen){
        return -ERROR_INVALID_ARGUMENTS;
    }

    /* Width and height are the content size, 16 px smaller than the window. */
    gfx_window_clamp_size(w, &width, &height);
    if(width <= 0 || height <= 0){
        return -ERROR_INVALID_ARGUMENTS;
    }
    if(width == w->inner_width && height == w->inner_height){
        return ERROR_OK;
    }

    /**
     * Allocate new inner buffer, copy over old buffer, free old buffer, update struct.
     * Window buffers can be large, so running out of memory here keeps the old size instead of panicking.
     */
    uint8_t* new_buffer = kalloc_try(width*height);
    if(new_buffer == NULL){
        warningf("Not enough memory to resize %s to %dx%d\n", w->name, width, height);
        return -ERROR_ALLOC;
    }

    /**
     * The owner may be drawing into the buffer on its own thread. Every draw holds buffer_lock,
     * so taking it waits for a draw in progress to finish, and keeps new ones out until the swap is done.
     */
    spin_lock(&w->buffer_lock);

    uint8_t* old = w->inner;

    /**
     * kalloc doesn't clear memory. Keep the part of the old content that still fits, and clear
     * the rest the way gfx_new_window does, so until the app redraws, the window shows its
     * last frame instead of leftover memory.
     */
    uint8_t fill = HAS_FLAG(w->flags, GFX_IS_TRANSPARENT) ? 255 : 0;
    int copy_width = MIN(width, w->inner_width);
    int copy_height = MIN(height, w->inner_height);
    for(int row = 0; row < height; row++){
        uint8_t* dest = new_buffer + row*width;
        int copied = 0;
        if(row < copy_height){
            memcpy(dest, old + row*w->pitch, copy_width);
            copied = copy_width;
        }
        memset(dest + copied, fill, width - copied);
    }

    w->inner_height = height;
    w->inner_width = width;
    w->width = width + 16;
    w->height = height + 16;
    w->pitch = width;

    /* Copy over */
    w->inner = new_buffer;

    spin_unlock(&w->buffer_lock);

    struct gfx_event e = {
        .data = width,
        .data2 = height,
        .event = GFX_EVENT_RESOLUTION
    };
    gfx_push_event(w, &e);

    w->changed = 1;

    /* Safe to free: any draw that started after the unlock uses the new buffer */
    kfree(old);

    return ERROR_OK;
}

/**
 * @brief Default handler for click event from mouse on given window.
 * 
 * @param window Window that was clicked.
 * @param x 
 * @param y 
 */
static void gfx_default_click(struct window* window, int x, int y)
{
    if(HAS_FLAG(window->flags, GFX_NO_OPTIONS)) return;
    
    if(gfx_point_in_rectangle(window->x, window->y, window->x+window->width, window->y+10, x, y)){
        //dbgprintf("Clicked %s header\n", window->name);
    }

    /* Exit button */
    if (x >= window->x + window->width - 22 + 6 && x <= window->x + window->width - 22 + 6 + 10 && y >= window->y - 3 && y <= window->y - 3 + 9) {
        dbgprintf("[GFX WINDOW] Clicked %s exit button\n", window->name);
        struct gfx_event e = {
            .data = 0,
            .data2 = 0,
            .event = GFX_EVENT_EXIT
        };
        gfx_push_event(window, &e);
        return; 
    }

    /* Minimize button */
    if (x >= window->x + window->width - 34 + 6 && x <= window->x + window->width - 34 + 6 + 10 && y >= window->y - 3 && y <= window->y - 3 + 9) {
        dbgprintf("[GFX WINDOW] Clicked %s minimize button\n", window->name);
        
        /* toggle IS_HIDDEN flag */
        if(HAS_FLAG(window->flags, GFX_IS_HIDDEN)){
            window->flags &= ~GFX_IS_HIDDEN;
        } else {
            window->flags |= GFX_IS_HIDDEN;
        }

        window->changed = true;
        return;
    }

    /* Full screen button */
    if (x >= window->x + window->width - 46 + 6 && x <= window->x + window->width - 46 + 6 + 10 && y >= window->y - 3 && y <= window->y - 3 + 9 && window->is_resizable) {
        dbgprintf("[GFX WINDOW] Clicked %s full screen button\n", window->name);
        if(window->is_maximized.state == 0){
            window->is_maximized.width = window->width+4;
            window->is_maximized.height = window->height;

            /* Without enough memory for the bigger buffer, the window stays as it is */
            if(window->ops->maximize(window) < 0){
                return;
            }
            window->is_maximized.state = 1;

        } else {
            /* resize sets width and height, and leaves them alone if it fails */
            if(window->ops->resize(window, window->is_maximized.width-16, window->is_maximized.height-16) < 0){
                return;
            }
            window->is_maximized.state = 0;
        }

        struct gfx_event e = {
            .data = window->inner_width,
            .data2 = window->inner_height,
            .event = GFX_EVENT_RESOLUTION
        };
        gfx_push_event(window, &e);

        window->is_moving.state = GFX_WINDOW_STATIC;

        window->resize = 0;

        return;
    }

}


static void gfx_default_hover(struct window* window, int x, int y)
{
    if(window->is_moving.state == GFX_WINDOW_MOVING && !HAS_FLAG(window->flags, GFX_IS_IMMUATABLE)){

        /**
         * Keep the point where the window was grabbed under the cursor. Each axis is clamped on its own,
         * so a window pushed against an edge slides along it instead of stopping.
         * Everything the window draws stays on screen, including the frame above window->y. Drawing is
         * clipped to the screen (#4), but a window cut off at an edge can't be dragged back; #18 covers that.
         */
        window->x = MAX(MIN(x - window->is_moving.x, vbe_info->width - window->width), 0);
        window->y = MAX(MIN(y - window->is_moving.y, vbe_info->height - window->height), GFX_WINDOW_HEADER_OVERHANG);

        window->changed = 1;
    }

    /**
     * Only the outline follows the cursor. Reallocating the buffer on every move fragmented the
     * small kernel heap, so gfx_default_mouse_up resizes it once, on release.
     */
    if(window->resize && window->is_resizable){
        int width = x-window->x-8;
        int height = y-window->y-8;
        gfx_window_clamp_size(window, &width, &height);
        if(width > 0 && height > 0){
            window->pending_size.width = width;
            window->pending_size.height = height;
            window->changed = 1;
        }
    }
}

static void gfx_default_mouse_down(struct window* window, int x, int y)
{
    if(gfx_point_in_rectangle(window->x, window->y-GFX_WINDOW_HEADER_OVERHANG, window->x+window->width-60, window->y+10, x, y)){
        window->is_moving.state = GFX_WINDOW_MOVING;
        window->is_moving.x = x - window->x;
        window->is_moving.y = y - window->y;
    }

    if(gfx_point_in_rectangle(window->x+window->width-8,  window->y+window->height-8, window->x+window->width,  window->y+window->height, x, y) && window->is_resizable){
        dbgprintf("Clicked resize\n");
        window->resize = 1;

        /* Until the cursor moves, the outline shows the current size */
        window->pending_size.width = window->inner_width;
        window->pending_size.height = window->inner_height;
    }
}

/* Only called for the window that got the press, so a release anywhere ends its drag or resize. */
static void gfx_default_mouse_up(struct window* window, int x, int y)
{
    if(window->is_moving.state == GFX_WINDOW_MOVING){
        window->is_moving.state = GFX_WINDOW_STATIC;

        window->changed = 1;
    }

    if(window->resize){
        dbgprintf("Unclicked resize\n");
        window->resize = 0;

        /* Resize the buffer once, to the size the outline showed. Without enough memory, the window keeps its old size. */
        window->ops->resize(window, window->pending_size.width, window->pending_size.height);
        window->changed = 1;
    }
}

int kernel_gfx_window_border_color(uint8_t color)
{
    if($process->current->gfx_window == NULL) return -1;
    $process->current->gfx_window->color.border = color;

    return ERROR_OK;
}

int gfx_destroy_window(struct window* w)
{
    ERR_ON_NULL(w);

    /* First, since once the window is queued for freeing, the window server may free it at any moment */
    w->owner->gfx_window = NULL;

    /**
     * Takes the window off the screen and out of the window lists. The window server may be partway through
     * a frame that uses it, so the window server frees it and its buffer itself, at the start of its next
     * frame (#16).
     */
    gfx_composition_remove_window(w);

    return ERROR_OK;
}


static int gfx_window_maximize(struct window* window)
{
    ERR_ON_NULL(window);

    int x = window->x, y = window->y;

    /* Move first, since resizing keeps the window on screen from where it is */
    window->ops->move(window, 0, 26);

    /* Placed at y 26, a window this tall ends exactly at the bottom of the screen */
    int ret = gfx_window_resize(window, vbe_info->width-16, vbe_info->height-42);
    if(ret < 0){
        window->ops->move(window, x, y);
        return ret;
    }

    window->changed = 1;

    return ERROR_OK;
}

/**
 * @brief Creates a new window and attaches it to the process who called the function.
 * Allocates the inner buffer in the processes personal memory.
 * @param width 
 * @param height 
 * @return struct window* 
 */
struct window* gfx_new_window(int width, int height, window_flag_t flags)
{
    if($process->current->gfx_window != NULL)
        return $process->current->gfx_window;

    struct window* w = create(struct window);
    if(w == NULL){
        warningf("window is NULL\n");
        return NULL;
    }

    /* kalloc_try, so running out of memory fails the new window instead of panicking. Apps exit when they don't get one. */
    w->inner = kalloc_try(width*height);
    if(w->inner == NULL){
        warningf("Inner window is NULL\n");
        kfree(w);
        return NULL;
    }
    memset(w->inner, HAS_FLAG(flags, GFX_IS_TRANSPARENT) ? 255 : 0, width*height);
    memset(w->events.list, 0, sizeof(struct gfx_event)*GFX_MAX_EVENTS);

    w->flags = flags;

    /* Set default ops */
    w->ops = &default_window_ops;
    w->draw = &default_window_draw_ops;

    w->inner_height = height;
    w->inner_width = width;

    w->width = width;
    w->height = height;

    if(!HAS_FLAG(w->flags, GFX_HIDE_HEADER)){
        w->width += 16;
        w->height += 16;
    }

    w->owner = $process->current;
    w->pitch = w->inner_width;
    w->x = 32;
    w->y = 32;

    w->changed = 1;

    w->color.border = 0;
    w->color.header = 0;
    w->color.text = 0;
    w->spinlock = 0;
    w->buffer_lock = SPINLOCK_UNLOCKED;

    w->next = NULL;

    if(flags & GFX_IS_RESIZABLE){
        w->is_resizable = 1;
    } else {
        w->is_resizable = 0;
    }
    w->resize = 0;
    w->is_fullscreen = 0;
    w->removed = 0;
    w->next_free = NULL;
    w->pending_size.width = 0;
    w->pending_size.height = 0;
    
    w->events.head = 0;
    w->events.tail = 0;

    w->is_maximized.state = 0;
    w->is_maximized.width = 0;
    w->is_maximized.height = 0;

    w->is_moving.state = GFX_WINDOW_STATIC;
    /* Window can just use the name of the owner? */
    memcpy(w->name, $process->current->name, strlen($process->current->name)+1);
    memset(w->header, 0, GFX_MAX_WINDOW_NAME_SIZE);
    w->in_focus = 0;

    dbgprintf("[Window] Created new window for %s at 0x%x: inner 0x%x (total %x - %x)\n", $process->current->name, w, w->inner, sizeof(struct window), width*height);

    $process->current->gfx_window = w;
    gfx_composition_add_window(w);

    return w;
}
