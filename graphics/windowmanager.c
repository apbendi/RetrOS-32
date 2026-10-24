/**
 * @file windowmanager.c
 * @author Joe Bayer (joexbayer)
 * @brief Window manager implementation
 * @version 0.1
 * @date 2023-11-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <windowmanager.h>
#include <libc.h>
#include <errors.h>
#include <gfx/window.h>
#include <gfx/gfxlib.h>
#include <gfx/events.h>
#include <sync.h>
#include <assert.h>
#include <memory.h>
#include <mouse.h>
#include <math.h>
#include <vbe.h>
#include <pcb.h>

/* default static prototype functions for windowmanager ops */

static int wm_default_add(struct windowmanager* wm, struct window* window);
static int wm_default_remove(struct windowmanager* wm, struct window* window);
static int wm_default_draw(struct windowmanager* wm, struct window* window);
static int wm_default_push_front(struct windowmanager* wm, struct window* window);
static int wm_default_push_back(struct windowmanager* wm, struct window* window);
static int wm_default_mouse_event(struct windowmanager* wm, int x, int y, char flags);
static int wm_default_workspace(struct windowmanager* wm, int workspace);
static int wm_default_changes(struct windowmanager* wm);

/* default windowmanager ops struct */
static struct windowmanager_ops wm_default_wm_ops = {
    .add = wm_default_add,
    .remove = wm_default_remove,
    .draw = wm_default_draw,
    .push_front = wm_default_push_front,
    .push_back = wm_default_push_back,
    .mouse_event = wm_default_mouse_event,
    .workspace = wm_default_workspace,
    .init = init_windowmanager,
    .destroy = wm_destroy,
    .changes = wm_default_changes
};

/* init new window manager with default ops and rest 0 */
int init_windowmanager(struct windowmanager* wm, int flags)
{
    ERR_ON_NULL(wm);

    wm->composition_buffer_size = VBE_SIZE();
    wm->composition_buffer = kalloc(VBE_SIZE());
    if(wm->composition_buffer == NULL){
        dbgprintf("Failed to allocate composition buffer\n");
        return -ERROR_ALLOC;
    }
    memset(wm->composition_buffer, 0x0, VBE_SIZE());


    wm->spinlock = SPINLOCK_UNLOCKED;
    wm->windows = NULL;
    wm->window_count = 0;
    wm->mouse_state = 0;
    wm->pressed_window = NULL;

    /* workspaces */
    wm->workspace = 0;
    for(int i = 0; i < WM_MAX_WORKSPACES; i++){
        wm->workspaces[i] = NULL;
    }

    /* flags */
    wm->flags = flags;
    
    /* state */
    wm->state = WM_INITIALIZED;

    dbgprintf("Initialized window manager\n");

    return ERROR_OK;
}

static int wm_default_changes(struct windowmanager* wm)
{
    ERR_ON_NULL(wm);
    WM_VALIDATE(wm);

    struct window* w = wm->windows;

    while(w != NULL){
        if(w->changed)
            return 1;
        w = w->next;
    }

    return 0;
}


/* default static functions for windowmanager ops */

/**
 * @brief wm_default_add adds a window to the windowmanager
 * adds the window to the end of the list
 * @param wm  windowmanager
 * @param window  window to add
 * @return int  0 on success, error code otherwise
 */
static int wm_default_add(struct windowmanager* wm, struct window* window)
{
    ERR_ON_NULL(wm);
    ERR_ON_NULL(window);

    /**
     * Under the lock: windows are added on whichever thread opens them, often several at once at boot, and
     * removed on the cleanup worker. Unlocked, two adds could both read the same front window, and the first
     * window added would be lost (#37).
     */
    spin_lock(&wm->spinlock);

    /* A window being destroyed is out of the lists for good (#16). A workspace switch re-adds the taskbar's window. */
    if(window->removed){
        spin_unlock(&wm->spinlock);
        return -ERROR_WINDOW_NOT_FOUND;
    }

    /* Whether this workspace's list is empty. Not window_count, which counts every workspace's windows. */
    struct window* current = wm->windows;

    /* Set next before publishing the window, so a walk without the lock never sees the list cut short */
    window->next = current;
    COMPILER_BARRIER();
    wm->windows = window;
    wm->window_count++;

    if(current == NULL){
        window->in_focus = 1;
    } else if(!HAS_FLAG(window->flags, GFX_HIDE_HEADER)){
        window->in_focus = 1;
        current->in_focus = 0;
    }

    spin_unlock(&wm->spinlock);

    return ERROR_OK;
}

static int wm_default_workspace(struct windowmanager* wm, int workspace)
{
    ERR_ON_NULL(wm);

    dbgprintf("Switching to workspace %d\n", workspace);

    if(workspace < 0 || workspace >= WM_MAX_WORKSPACES){
        return -ERROR_INVALID_ARGUMENTS;
    }

    /* Under the lock, so a window being removed on another thread is found in exactly one list */
    spin_lock(&wm->spinlock);

    /* store current windows in workspace */
    wm->workspaces[wm->workspace] = wm->windows;

    /* set new workspace */
    wm->windows = wm->workspaces[workspace];
    wm->workspace = workspace;

    spin_unlock(&wm->spinlock);

    return ERROR_OK;
}

/**
 * Takes a window out of whichever list holds it: the active one, or another workspace's saved list. The caller
 * holds wm->spinlock. The window keeps its next pointer, so a walk that's on it still reaches the rest of the list.
 * @return ERROR_OK, or -ERROR_WINDOW_NOT_FOUND if it wasn't in any list
 */
static int __wm_unlink(struct windowmanager* wm, struct window* window)
{
    /* -1 is the active list. The active workspace's saved slot is stale until the next switch, so it's skipped. */
    for(int workspace = -1; workspace < WM_MAX_WORKSPACES; workspace++){
        if(workspace == wm->workspace){
            continue;
        }
        struct window** link = workspace < 0 ? &wm->windows : &wm->workspaces[workspace];
        for(; *link != NULL; link = &(*link)->next){
            if(*link == window){
                *link = window->next;
                return ERROR_OK;
            }
        }
    }
    return -ERROR_WINDOW_NOT_FOUND;
}

/**
 * @brief wm_default_remove removes a window from the windowmanager
 * 
 * @param wm windowmanager
 * @param window window to remove
 * @return int 0 on success, -ERROR_WINDOW_NOT_FOUND if it wasn't in any list
 */
static int wm_default_remove(struct windowmanager* wm, struct window* window)
{
    ERR_ON_NULL(wm);
    ERR_ON_NULL(window);

    spin_lock(&wm->spinlock);

    if(wm->windows == window){
        dbgprintf("Removing front window %s\n", window->name);
    } else {
        dbgprintf("Removing window %s\n", window->name);
    }

    /* A window on another workspace is in that workspace's saved list, so look there too */
    int ret = __wm_unlink(wm, window);
    if(ret == ERROR_OK){
        wm->window_count--;
    }

    if(wm->windows) {
        wm->windows->changed = 1;
        wm->windows->in_focus = 1;
    }

    spin_unlock(&wm->spinlock);

    return ret;
}

static int wm_default_push_back(struct windowmanager* wm, struct window* window)
{
    ERR_ON_NULL(wm);

    if(wm->window_count < 2){
        return ERROR_OK;
    }

    dbgprintf("Pushing window %s to back\n", window->name);

    spin_lock(&wm->spinlock);

    /**
     * Unlink and relink under one hold of the lock. A window destroyed since the caller found it is out of the
     * lists for good, and waiting to be freed (#16), so it must not be put back.
     */
    if(window->removed || __wm_unlink(wm, window) < 0){
        spin_unlock(&wm->spinlock);
        return ERROR_OK;
    }

    window->in_focus = 0;

    struct window* i;
    for (i = wm->windows; i != NULL && i->next != NULL; i = i->next);
    if (i){
        i->next = window;
    } else {
        wm->windows = window;
    }

    window->next = NULL;
    window->changed = 1;

    /**
     * The taskbar comes to the front only when it's clicked, for its menus. If it's in front now, it goes behind the
     * window just pushed back, so Alt+Tab moves to the next window rather than to the taskbar (#89). Only the window
     * server thread calls push_back, so nothing walks the list meanwhile.
     */
    struct window* front = wm->windows;
    if(front != window && HAS_FLAG(front->flags, GFX_HIDE_HEADER)){
        wm->windows = front->next;
        front->next = NULL;
        window->next = front;
    }

    if(wm->windows){
        wm->windows->changed = 1;
        wm->windows->in_focus = 1;
    }

    spin_unlock(&wm->spinlock);

    return ERROR_OK;
}

/**
 * @brief wm_default_push_front pushes a window to the front of the windowmanager
 * Used to bring a window to the front of the screen
 * @param wm windowmanager
 * @param window window to push 
 * @return int 0 on success, error code otherwise 
 */
static int wm_default_push_front(struct windowmanager* wm, struct window* window)
{
    ERR_ON_NULL(wm);

    if(window == wm->windows){
        return ERROR_OK;
    }

    dbgprintf("Pushing window %s to front\n", window->name);

    spin_lock(&wm->spinlock);

    /* As in push_back: a destroyed window must not be put back */
    if(window->removed || __wm_unlink(wm, window) < 0){
        spin_unlock(&wm->spinlock);
        return ERROR_OK;
    }

    if(!HAS_FLAG(window->flags, GFX_HIDE_HEADER) && wm->windows != NULL){
        wm->windows->in_focus = 0;
    }
    /* As in add: next first, so a walk without the lock never sees the list cut short */
    window->next = wm->windows;
    COMPILER_BARRIER();
    wm->windows = window;

    if(!HAS_FLAG(window->flags, GFX_HIDE_HEADER)){
        wm->windows->in_focus = 1;
    }

    window->changed = 1;

    spin_unlock(&wm->spinlock);

    return ERROR_OK;

}

/**
 * @brief wm_default_draw draws the windows from back to front recursively
 * TODO: add a max depth to this to prevent stack overflow
 * @warning this can cause a stack overflow if there are too many windows
 * @param wm windowmanager
 * @param window window to recursively draw from 
 * @return int 0 on success, error code otherwise
 */
static int wm_default_draw(struct windowmanager* wm, struct window* window)
{
    ERR_ON_NULL(wm);
    ERR_ON_NULL(window);

    if (wm->window_count == 0 || window == NULL) {
        return ERROR_OK;
    }
    if(window->next != NULL){
        wm->ops->draw(wm, window->next);
    }
    
    window->draw->draw(wm->composition_buffer, window);

    return ERROR_OK;
}

/**
 * Sends a press, move or release at x, y on the screen to w, relative to its content, and with the modifier keys held.
 * Outside the content, that's negative or past its size, so it's stored as an int16_t.
 */
static void __wm_push_mouse(struct window* w, uint8_t event, int x, int y)
{
    int offset = HAS_FLAG(w->flags, GFX_HIDE_HEADER) ? 0 : 8;
    struct gfx_event e = {
        .data = (uint16_t)(int16_t)(x - (w->x + offset)),
        .data2 = (uint16_t)(int16_t)(y - (w->y + offset)),
        .flags = kb_get_modifiers(),
        .event = event
    };
    gfx_push_event(w, &e);
}

/* Whether x, y on the screen is over w's content, not its title bar or border */
static bool_t __wm_over_content(struct window* w, int x, int y)
{
    if(HAS_FLAG(w->flags, GFX_IS_HIDDEN)) return false;

    int offset = HAS_FLAG(w->flags, GFX_HIDE_HEADER) ? 0 : 8;
    int cx = x - (w->x + offset);
    int cy = y - (w->y + offset);
    return cx >= 0 && cy >= 0 && cx < w->inner_width && cy < w->inner_height;
}

static int wm_default_mouse_event(struct windowmanager* wm, int x, int y, char flags)
{
    ERR_ON_NULL(wm);

    if (wm->window_count == 0) {
        return ERROR_OK;
    }
    /**
     * Walks the list without the lock. Other threads do add and remove windows meanwhile, but a removed window
     * keeps its next pointer, and isn't freed until the window server's next frame (#16).
     */

    /* find the window under the mouse, from front to back. */
    struct window* window = NULL;
    struct window* moving = NULL;
    uint16_t x2 = 0, y2 = 0;
    for (struct window* i = wm->windows; i != NULL; i = i->next){
        /* if mouse is in window, including the title bar drawn above it */
        int top = HAS_FLAG(i->flags, GFX_HIDE_HEADER) ? i->y : i->y-GFX_WINDOW_HEADER_OVERHANG;
        int bottom = i->y+i->height;

        /* A minimized window only draws its title bar, so only the title bar takes the mouse */
        if(HAS_FLAG(i->flags, GFX_IS_HIDDEN)){
            if(HAS_FLAG(i->flags, GFX_HIDE_HEADER)) continue;
            bottom = top+GFX_WINDOW_TITLE_HEIGHT;
        }

        if(gfx_point_in_rectangle(i->x, top, i->x+i->width, bottom, x, y)){
            /* get coordinates inside of the window */
            int offset = HAS_FLAG(i->flags, GFX_HIDE_HEADER) ? 0 : 8;
            x2 = CLAMP( (x - (i->x+offset)), 0,  i->inner_width);
            y2 = CLAMP( (y - (i->y+offset)), 0,  i->inner_height);

            /* check if the pixel is transparent (255) */
            if(WINDOW_GET_PIXEL(i, x2, y2) == 255 && HAS_FLAG(i->flags, GFX_IS_TRANSPARENT)){
                continue;
            }

            window = i;
            break;
        }
        /**
         * @brief Special edge case:
         * When a window is moving or resizing (only 1 window should be at a time)
         * and the mouse is not in the window, the window should snap to the mouse.
         * A window keeps its old size until a resize is released, so while it grows the mouse is always outside it.
         */
        if(i->is_moving.state == GFX_WINDOW_MOVING || i->resize){
            moving = i;
            break;
        }
    }

    /* A release goes to the window that got the press, wherever the mouse is now. */
    if(!(flags & MOUSE_LEFT) && wm->mouse_state == 1){
        struct window* pressed = wm->pressed_window;
        bool_t dragging = wm->dragging;
        wm->mouse_state = 0;
        wm->pressed_window = NULL;
        wm->dragging = 0;

        if(pressed != NULL){
            if(dragging){
                __wm_push_mouse(pressed, GFX_EVENT_MOUSE_UP, x, y);
            }

            /* It's only a click if the mouse is still over the pressed window. */
            if(pressed == window){
                pressed->ops->click(pressed, x, y);
            }
            pressed->ops->mouseup(pressed, x, y);

            /* Send mouse event to all windows that are not hidden */
            if(pressed == window && !HAS_FLAG(pressed->flags, GFX_IS_HIDDEN)){
                struct gfx_event e = {
                    .data = x2,
                    .data2 = y2,
                    .event = GFX_EVENT_MOUSE
                };
                gfx_push_event(pressed, &e);
            }
        }
    }

    /**
     * While the button is held after a press on a window's content, moves go to that window, wherever the mouse is.
     * pressed_window is cleared if the window is destroyed, and it isn't freed before the next frame (#16).
     */
    struct window* pressed = wm->pressed_window;
    if((flags & MOUSE_LEFT) && wm->dragging && pressed != NULL && (x != wm->drag_x || y != wm->drag_y)){
        wm->drag_x = x;
        wm->drag_y = y;
        __wm_push_mouse(pressed, GFX_EVENT_MOUSE_MOVE, x, y);
    }

    if(moving != NULL){
        moving->ops->hover(moving, x, y);
        return ERROR_OK;
    }

    if(window == NULL){
        return ERROR_OK;
    }

    /* on click when left mouse down */
    if((flags & MOUSE_LEFT) && wm->mouse_state == 0){
        wm->mouse_state = 1;

        /**
         * The window may have been destroyed since the walk above found it. Then it's waiting to be freed (#16),
         * so don't keep a pointer to it past this frame. ws_remove sets removed and clears pressed_window with
         * interrupts off, so checking and setting with interrupts off sees one or the other.
         */
        ENTER_CRITICAL();
        if(!window->removed){
            wm->pressed_window = window;
        }
        LEAVE_CRITICAL();
        window->ops->mousedown(window, x, y);

        /* A press on the content, rather than one that grabbed the title bar or the corner, starts a drag for the window */
        if(wm->pressed_window == window && window->is_moving.state != GFX_WINDOW_MOVING && !window->resize && __wm_over_content(window, x, y)){
            wm->dragging = 1;
            wm->drag_x = x;
            wm->drag_y = y;
            __wm_push_mouse(window, GFX_EVENT_MOUSE_DOWN, x, y);
        }

        /* If clicked window is not in front, push it. */
        if(window != wm->windows){
            wm->ops->push_front(wm, window);
        }
    }

    /* always send a hover event */
    window->ops->hover(window, x, y);

    return ERROR_OK;
}

/**
 * @brief Copies the current workspace's windows, front to back, so they can be listed after the lock is released
 * 
 * @param wm windowmanager
 * @param out where to copy up to max windows
 * @param max how many fit in out
 * @return int how many windows there are, which can be more than max
 */
int wm_get_windows(struct windowmanager* wm, struct wm_window_info* out, int max)
{
    ERR_ON_NULL(wm);
    ERR_ON_NULL(out);

    int count = 0;
    spin_lock(&wm->spinlock);
    for (struct window* i = wm->windows; i != NULL; i = i->next){
        if(count < max){
            struct wm_window_info* info = &out[count];
            memcpy(info->name, i->name, GFX_MAX_WINDOW_NAME_SIZE);
            info->name[GFX_MAX_WINDOW_NAME_SIZE-1] = 0;
            info->x = i->x;
            info->y = i->y;
            info->width = i->width;
            info->height = i->height;
            info->pid = i->owner != NULL ? i->owner->pid : -1;
            info->flags = i->flags;
            info->maximized = i->is_maximized.state;
        }
        count++;
    }
    spin_unlock(&wm->spinlock);

    return count;
}

/**
 * @brief Logs the current workspace's windows to the serial log, front to back, one line each:
 * <index> "<title>" x=<x> y=<y> w=<width> h=<height> pid=<owner>, then any of
 * minimized, maximized, no-title-bar and transparent.
 * x, y, w and h are the window's outer rectangle. The title bar is drawn GFX_WINDOW_HEADER_OVERHANG
 * pixels above y and is GFX_WINDOW_TITLE_HEIGHT tall, and a minimized window shows only its title bar.
 * The F12 key calls this, so tools can find windows whatever has focus.
 * 
 * @param wm windowmanager
 */
void wm_log_windows(struct windowmanager* wm)
{
    struct wm_window_info windows[WM_LIST_MAX_WINDOWS];
    int count = wm_get_windows(wm, windows, WM_LIST_MAX_WINDOWS);
    if(count < 0){
        return;
    }

    dbgprintf("%d windows in workspace %d, front to back\n", count, wm->workspace);
    for (int i = 0; i < count && i < WM_LIST_MAX_WINDOWS; i++){
        struct wm_window_info* w = &windows[i];
        dbgprintf("%d \"%s\" x=%d y=%d w=%d h=%d pid=%d%s%s%s%s\n", i, w->name, w->x, w->y, w->width, w->height, w->pid,
            HAS_FLAG(w->flags, GFX_IS_HIDDEN) ? " minimized" : "",
            w->maximized ? " maximized" : "",
            HAS_FLAG(w->flags, GFX_HIDE_HEADER) ? " no-title-bar" : "",
            HAS_FLAG(w->flags, GFX_IS_TRANSPARENT) ? " transparent" : ""
        );
    }
    if(count > WM_LIST_MAX_WINDOWS){
        dbgprintf("and %d more\n", count - WM_LIST_MAX_WINDOWS);
    }
}

/**
 * @brief wm_init initializes a windowmanager
 * 
 * @param wm windowmanager
 * @return int 0 on success, error code otherwise
 */
struct windowmanager* wm_new(int flags)
{
    struct windowmanager* wm = create(struct windowmanager);
    if(wm == NULL){
        return NULL;
    }
    /* init krefs */
    kref_init(&wm->_krefs);
    
    wm->ops = &wm_default_wm_ops;

    /* init windowmanager */
    if(wm->ops->init(wm, flags) < 0){
        kfree(wm);
        return NULL;
    }

    kref_get(&wm->_krefs);
    return wm;
}

/**
 * @brief wm_destroy destroys a windowmanager
 * 
 * @param wm windowmanager
 * @return int 0 on success, error code otherwise
 */
int wm_destroy(struct windowmanager* wm)
{
    ERR_ON_NULL(wm);
    WM_VALIDATE(wm);

    /* check refs */
    if(kref_put(&wm->_krefs) > 0){
        return ERROR_OK;
    }

    /* free composition buffer */
    if(wm->composition_buffer != NULL){
        kfree(wm->composition_buffer);
    }

    /* free workspaces */
    // TODO

    /* free windows */

    /* free windowmanager */
    kfree(wm);

    return ERROR_OK;
}