/**
 * @file windowserver.c
 * @author Joe Bayer (joexbayer)
 * @brief Windowserver implementation
 * @version 0.1
 * @date 2023-11-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <gfx/windowserver.h>
#include <keyboard.h>
#include <gfx/gfxlib.h>
#include <scheduler.h>
#include <gfx/events.h>
#include <memory.h>
#include <fs/fs.h>
#include <vbe.h>
#include <colors.h>
#include <math.h>

static int ws_init(struct windowserver* ws);
static int ws_add(struct windowserver* ws, struct window* window);
static int ws_remove(struct windowserver* ws, struct window* window);
static int ws_fullscreen(struct windowserver* ws, struct window* window);
static struct window* __ws_enter_fullscreen(struct windowserver* ws, struct window* window,
                                            int mouse_x, int mouse_y, int* rel_x, int* rel_y);
static struct window* __ws_leave_fullscreen(struct windowserver* ws, int mouse_x, int mouse_y,
                                            int* rel_x, int* rel_y);
static int ws_set_background(struct windowserver* ws, color_t color);
static int ws_draw(struct windowserver* ws);
static int ws_destroy(struct windowserver* ws);

static struct window_server_ops ws_default_ops = {
    .init = ws_init,
    .add = ws_add,
    .remove = ws_remove,
    .fullscreen = ws_fullscreen,
    .set_background = ws_set_background,
    .draw = ws_draw,
    .destroy = ws_destroy
};

static int ws_init(struct windowserver* ws)
{
    ERR_ON_NULL(ws);

    dbgprintf("[WSERVER] Initializing window manager.\n");

    ws->sleep_time = WINDOW_SERVER_SLEEP_TIME;
    ws->_fullscreen = NULL;
    ws->_free = NULL;

    struct windowmanager* wm = wm_new(0);
    if(wm == NULL){
        ws->ops->destroy(ws);
        return -ERROR_ALLOC;
    }
    ws->_wm = wm;
    ws->workspace = 0;

    SET_FlAG(ws->flags, WINDOW_SERVER_INITIALIZED);
    return ERROR_OK;
}

static int ws_add(struct windowserver* ws, struct window* window)
{
    ERR_ON_NULL(ws);
    ERR_ON_NULL(window);
    WS_VALIDATE(ws);

    if(ws->_wm->ops->add(ws->_wm, window) < 0){
        return -ERROR_OPS_CORRUPTED;
    }

    dbgprintf("[WSERVER] Added window %x.\n", window);

    return ERROR_OK;
}

static int ws_remove(struct windowserver* ws, struct window* window)
{
    ERR_ON_NULL(ws);
    ERR_ON_NULL(window);
    WS_VALIDATE(ws);

    /**
     * Only reached when the window is destroyed. This runs on the cleanup worker, while the window server may be
     * partway through a frame that uses the window, so the window is freed later, by the window server (#16).
     *
     * With interrupts off, so the window server sees all of these or none of them:
     * - removed keeps the window from being put back in a list, made pressed_window or made fullscreen.
     * - pressed_window is cleared, with any drag it had, so a later move or release doesn't reach it, and so is
     *   click_window, the last press's. Not done in wm remove, which isn't only for destroyed windows.
     * - A fullscreen window gets its own buffer back, the one that's freed later, instead of the composition
     *   buffer, and the desktop is composited again (#17). Its owner has exited, so nothing draws into it,
     *   and buffer_lock isn't needed.
     */
    ENTER_CRITICAL();
    window->removed = 1;
    if(ws->_wm->pressed_window == window){
        ws->_wm->pressed_window = NULL;
        ws->_wm->dragging = 0;
    }
    if(ws->_wm->click_window == window){
        ws->_wm->click_window = NULL;
    }
    if(ws->_fullscreen == window){
        int rel_x, rel_y;
        __ws_leave_fullscreen(ws, ws->m.x+VESA_CURSOR_HOTSPOT_X, ws->m.y+VESA_CURSOR_HOTSPOT_Y, &rel_x, &rel_y);
    }
    LEAVE_CRITICAL();

    /* Takes wm->spinlock, so not with interrupts off: spin_lock waits by yielding */
    if(ws->_wm->ops->remove(ws->_wm, window) < 0){
        warningf("[WSERVER] Window %s wasn't in any window list\n", window->name);
    }

    ENTER_CRITICAL();
    window->next_free = ws->_free;
    ws->_free = window;
    LEAVE_CRITICAL();

    return ERROR_OK;
}

/**
 * Frees the windows destroyed since the last frame. Called at the start of a frame, when the window server holds
 * no window pointers, and the windows are out of every list.
 */
static void __ws_free_windows(struct windowserver* ws)
{
    ENTER_CRITICAL();
    struct window* window = ws->_free;
    ws->_free = NULL;
    LEAVE_CRITICAL();

    while(window != NULL){
        struct window* next = window->next_free;
        kfree(window->inner);
        kfree(window);
        window = next;
    }
}

/**
 * Points the window at the composition buffer, at screen size, and saves what it had. Returns the window to send a
 * MOUSE_UP to if a drag was in progress, with its coordinates in rel_x and rel_y.
 */
static struct window* __ws_enter_fullscreen(struct windowserver* ws, struct window* window,
                                            int mouse_x, int mouse_y, int* rel_x, int* rel_y)
{
    struct window* drag = wm_reset_mouse(ws->_wm, mouse_x, mouse_y, rel_x, rel_y);

    ws->_saved.inner = window->inner;
    ws->_saved.inner_width = window->inner_width;
    ws->_saved.inner_height = window->inner_height;
    ws->_saved.pitch = window->pitch;
    ws->_saved.x = window->x;
    ws->_saved.y = window->y;

    window->inner = ws->_wm->composition_buffer;
    window->inner_width = vbe_info->width;
    window->inner_height = vbe_info->height;
    window->pitch = vbe_info->width;
    window->x = 0;
    window->y = 0;
    window->is_fullscreen = 1;

    ws->_fullscreen = window;

    return drag;
}

/**
 * Gives the fullscreen window back what it had before, and composites the desktop again. Returns the window to send
 * a MOUSE_UP to if a drag was in progress, with its coordinates in rel_x and rel_y.
 */
static struct window* __ws_leave_fullscreen(struct windowserver* ws, int mouse_x, int mouse_y,
                                            int* rel_x, int* rel_y)
{
    struct window* window = ws->_fullscreen;
    struct window* drag = wm_reset_mouse(ws->_wm, mouse_x, mouse_y, rel_x, rel_y);

    window->inner = ws->_saved.inner;
    window->inner_width = ws->_saved.inner_width;
    window->inner_height = ws->_saved.inner_height;
    window->pitch = ws->_saved.pitch;
    window->x = ws->_saved.x;
    window->y = ws->_saved.y;
    window->is_fullscreen = 0;
    window->changed = 1;

    ws->_fullscreen = NULL;
    ws->window_changes = 1;

    return drag;
}

/**
 * Makes the window fullscreen, or gives it back its own buffer if it already is.
 * Only one window can be fullscreen at a time, so for any other window this fails.
 */
static int ws_fullscreen(struct windowserver* ws, struct window* window)
{
    ERR_ON_NULL(ws);
    ERR_ON_NULL(window);
    WS_VALIDATE(ws);

    if(ws->_fullscreen != NULL && ws->_fullscreen != window){
        return -ERROR_INVALID_ARGUMENTS;
    }

    /**
     * Windows that can't be moved or resized, like the taskbar, don't go fullscreen either. The taskbar
     * doesn't draw over the whole screen, so the display would just look frozen until the next F10.
     */
    if(ws->_fullscreen == NULL && HAS_FLAG(window->flags, GFX_IS_IMMUATABLE)){
        return -ERROR_INVALID_ARGUMENTS;
    }

    int mouse_x = ws->m.x+VESA_CURSOR_HOTSPOT_X;
    int mouse_y = ws->m.y+VESA_CURSOR_HOTSPOT_Y;
    int rel_x = 0, rel_y = 0;
    struct window* drag = NULL;

    /* The window's owner may be drawing into it on its own thread, so swap buffers between its draws */
    spin_lock(&window->buffer_lock);

    /* ws_remove runs on another thread with interrupts off, so keep it from seeing a half-done swap */
    ENTER_CRITICAL();
    if(window->removed){
        /* Destroyed since F10 picked it: it's waiting to be freed, so it mustn't become fullscreen */
        LEAVE_CRITICAL();
        spin_unlock(&window->buffer_lock);
        return -ERROR_WINDOW_NOT_FOUND;
    }
    if(ws->_fullscreen == window){
        drag = __ws_leave_fullscreen(ws, mouse_x, mouse_y, &rel_x, &rel_y);
    } else {
        drag = __ws_enter_fullscreen(ws, window, mouse_x, mouse_y, &rel_x, &rel_y);
    }
    LEAVE_CRITICAL();

    spin_unlock(&window->buffer_lock);

    /* A drag the swap interrupted still needs its release, before the window gets its resolution event */
    if(drag != NULL){
        struct gfx_event e = {
            .data = (uint16_t)(int16_t)rel_x,
            .data2 = (uint16_t)(int16_t)rel_y,
            .flags = kb_get_modifiers(),
            .event = GFX_EVENT_MOUSE_UP
        };
        gfx_push_event(drag, &e);
    }

    return ERROR_OK;
}

static int ws_set_background(struct windowserver* ws, color_t color)
{
    ERR_ON_NULL(ws);
    WS_VALIDATE(ws);

    /* Shown from the next frame that composites the screen */
    ws->background = color;
    return ERROR_OK;
}

static int __ws_key_event(struct windowserver* ws, unsigned char key, unsigned char modifiers)
{
    ERR_ON_NULL(ws);
    WS_VALIDATE(ws);


    if(key == 0){return ERROR_OK;}

    switch (key){
    case ALT_1:
    case ALT_2:
    case ALT_3:
    case ALT_4:{
            /* While fullscreen, keys belong to the fullscreen window. F10 is how to leave it. */
            if(ws->_fullscreen != NULL){
                break;
            }

            /* Alt+1 to Alt+4 pick a workspace. F4 used to cycle through them, but apps need F4 (#89). */
            int workspace = key - ALT_1;
            if(workspace == ws->workspace || workspace >= WM_MAX_WORKSPACES){
                break;
            }

            if(ws->taskbar != NULL){
                ws->_wm->ops->remove(ws->_wm, ws->taskbar->gfx_window);
            }

            ws->workspace = workspace;
            ws->_wm->ops->workspace(ws->_wm, ws->workspace);
            ws->window_changes = 1;
            
            /* Behind the workspace's windows, so the front one has the keyboard rather than the taskbar */
            if(ws->taskbar != NULL){
                ws->_wm->ops->add(ws->_wm, ws->taskbar->gfx_window);
                ws->_wm->ops->push_back(ws->_wm, ws->taskbar->gfx_window);
            }
        }
        break;
    case ALT_F4:{
            /* What the close button does. Windows without one, the taskbar and the login and message boxes, stay. */
            struct window* w = ws->_fullscreen != NULL ? ws->_fullscreen : ws->_wm->windows;
            if(w == NULL || HAS_FLAG(w->flags, GFX_HIDE_HEADER) || HAS_FLAG(w->flags, GFX_NO_OPTIONS)){
                break;
            }

            dbgprintf("[WSERVER] Closing %s.\n", w->name);
            struct gfx_event e = {
                .data = 0,
                .data2 = 0,
                .event = GFX_EVENT_EXIT
            };
            gfx_push_event(w, &e);
        }
        break;
    case F10:{
            /**
             * Leave fullscreen on the fullscreen window, even if another window has come to the front since:
             * a click, Alt+Tab or the fullscreen window's exit can change the front window. Otherwise make the
             * front window fullscreen.
             */
            struct window* w = ws->_fullscreen != NULL ? ws->_fullscreen : ws->_wm->windows;
            if(w == NULL || ws->ops->fullscreen(ws, w) < 0){
                break;
            }

            struct gfx_event e = {
                .data = w->inner_width,
                .data2 = w->inner_height,
                .event = GFX_EVENT_RESOLUTION
            };
            gfx_push_event(w, &e);
        }
        break;
    case F12:
        /* Log the window list, for finding windows without a screenshot */
        wm_log_windows(ws->_wm);
        break;
    case ALT_TAB: {
            /* While fullscreen, keys belong to the fullscreen window. F10 is how to leave it. */
            if(ws->_fullscreen != NULL){
                break;
            }
            dbgprintf("[WSERVER] Switching focus.\n");
            struct windowmanager* wm = ws->_wm;
            wm->ops->push_back(wm, wm->windows);
        }
        break;
    default: {
            /* sends keyboard event to userspace, to the fullscreen window if there is one */
            struct gfx_event e = {
                .data = key,
                .flags = modifiers,
                .event = GFX_EVENT_KEYBOARD
            };
            gfx_push_event(ws->_fullscreen != NULL ? ws->_fullscreen : ws->_wm->windows, &e);
        }
        break;
    }

    return ERROR_OK;
}

/**
 * Copies a fullscreen frame to the framebuffer, with the cursor in it. The window's owner draws into the composition
 * buffer itself, so the cursor can't be left there: its pixels are saved, the cursor drawn, the frame copied, and the
 * pixels put back. Interrupts are off, so the owner can't draw while its buffer has the cursor in it (#165).
 */
static void __ws_copy_fullscreen(struct windowserver* ws)
{
    uint8_t* buffer = ws->_wm->composition_buffer;
    int x = ws->m.x;
    int y = ws->m.y;
    uint8_t saved[16][16];

    for(int l = 0; l < 16; l++){
        for(int i = 0; i < 16; i++){
            if(vesa_on_screen(x+i, y+l)){
                saved[l][i] = *getpixel(buffer, x+i, y+l, vbe_info->pitch);
            }
        }
    }

    vesa_put_icon16(buffer, x, y);
    xmemcpy((uint8_t*)vbe_info->framebuffer, buffer, ws->_wm->composition_buffer_size);

    for(int l = 0; l < 16; l++){
        for(int i = 0; i < 16; i++){
            if(vesa_on_screen(x+i, y+l)){
                putpixel(buffer, x+i, y+l, saved[l][i], vbe_info->pitch);
            }
        }
    }
}

static int ws_draw(struct windowserver* ws)
{
    ERR_ON_NULL(ws);
    WS_VALIDATE(ws);

    /* Before anything in this frame takes a window pointer */
    __ws_free_windows(ws);
    
    /* get state variables */
    int mouse_changed = mouse_get_event(&ws->m);
    ws->window_changes = ws->_wm->ops->changes(ws->_wm);
    struct kb_key key;
    if(kb_get_key(&key)){
        __ws_key_event(ws, key.key, key.modifiers);
    }

    /* Read after the keys, since F10 can change it in this frame */
    struct window* fullscreen = ws->_fullscreen;

    if((ws->window_changes || mouse_changed ) && fullscreen == NULL){
        memset(ws->_wm->composition_buffer, ws->background, ws->_wm->composition_buffer_size);

        /* write current workspace at the right bottom of screen, numbered from 1 like Alt+1 to Alt+4 */
        vesa_printf(ws->_wm->composition_buffer, 8, vbe_info->height -8, 0x1F, "Workspace %d", ws->workspace + 1);

        ws->_wm->ops->draw(ws->_wm, ws->_wm->windows);
        vesa_put_icon16(ws->_wm->composition_buffer, ws->m.x, ws->m.y);
    }

    /* Move out of this module */
    kernel_yield();

    ENTER_CRITICAL();
    /* Read again: the window may have been removed during the yield above, which leaves fullscreen */
    struct window* now = ws->_fullscreen;
    if(now == NULL){
        /* Copy buffer over to framebuffer. */
        xmemcpy((uint8_t*)vbe_info->framebuffer, ws->_wm->composition_buffer, ws->_wm->composition_buffer_size);
    } else if(now->changed || mouse_changed){
        /**
         * A fullscreen window draws into the composition buffer itself, so copy it only once the frame is done.
         * Copying mid-draw would put half-drawn frames on screen, making a selection drag or a full redraw flicker.
         *
         * A terminal draws a whole frame in one commit, with drawing set meanwhile, so it's copied as soon as
         * changed is set. That's needed when a program draws into a shell's terminal while the shell is running,
         * waiting for it in pcb_await. Any other window is copied once its owner waits for input or sleeps.
         *
         * The cursor goes into the copy, so it doesn't flash (#165).
         */
        bool_t ready = now->frames_atomic ? !now->drawing
                                          : (now->owner == NULL || now->owner->state != RUNNING);
        if(ready){
            now->changed = 0;
            __ws_copy_fullscreen(ws);
        }
    }
    LEAVE_CRITICAL();

    if(mouse_changed){
        /* internal mouse event for windows, at the cursor's arrow tip */
        int x = ws->m.x+VESA_CURSOR_HOTSPOT_X;
        int y = ws->m.y+VESA_CURSOR_HOTSPOT_Y;
        if(fullscreen != NULL){
            wm_fullscreen_mouse_event(ws->_wm, fullscreen, x, y, ws->m.flags);
        } else {
            ws->_wm->ops->mouse_event(ws->_wm, x, y, ws->m.flags);
        }
    }

    return ERROR_OK;
}

struct windowserver* ws_new()
{
    struct windowserver* ws = create(struct windowserver);
    if(ws == NULL){
        return NULL;
    }

    ws->ops = &ws_default_ops;
    kref_init(&ws->_krefs);

    if(ws->ops->init(ws) < 0){
        kfree(ws);
        return NULL;
    }

    ws_set_background(ws, 0x17);

    kref_get(&ws->_krefs);
    return ws;
}

static int ws_destroy(struct windowserver* ws)
{
    ERR_ON_NULL(ws);
    WS_VALIDATE(ws);

    if(kref_put(&ws->_krefs) > 0){
        return ERROR_OK;
    }

    ws->_wm->ops->destroy(ws->_wm);

    kfree(ws);
    return ERROR_OK;
}
