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
static void __ws_damage(struct gfx_rect* list, int* count, struct gfx_rect rect);

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

    /**
     * The screen copy starts cleared, as the composition buffer does, and so does the framebuffer, which holds the
     * boot messages until now. From here on only the window server writes the framebuffer, apart from the panic and
     * shutdown screens, which halt.
     */
    ws->_screen = kalloc(wm->composition_buffer_size);
    if(ws->_screen == NULL){
        wm->ops->destroy(wm);
        return -ERROR_ALLOC;
    }
    memset(ws->_screen, 0, wm->composition_buffer_size);
    memset((uint8_t*)vbe_info->framebuffer, 0, wm->composition_buffer_size);
    ws->_screen_cursor_x = 0;
    ws->_screen_cursor_y = 0;

    /* The first frame composites the desktop, even with no windows */
    ws->window_changes = 1;

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
    /* Where it was drawn shows what's behind it from the next frame (#180) */
    __ws_damage(ws->_damage, &ws->_damage_count, window->drawn);
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

    /* Shown from the next frame outside fullscreen */
    ws->background = color;
    ws->window_changes = 1;
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

/* The most words a row of the cursor can touch: its width, starting anywhere in a word */
#define WS_CURSOR_WORDS ((VESA_CURSOR_WIDTH + 6) / 4)

/* Writes each of count words of source that differs from the screen copy to the copy and the framebuffer */
static void __ws_copy_words(uint32_t* framebuffer, uint32_t* screen, const uint32_t* source, int count)
{
    for(int i = 0; i < count; i++){
        uint32_t word = source[i];
        if(word != screen[i]){
            screen[i] = word;
            framebuffer[i] = word;
        }
    }
}

/* The words a row of the cursor touches, first up to end, with the cursor drawn over them. Empty off screen. */
struct ws_cursor_row {
    int first, end;
    uint32_t words[WS_CURSOR_WORDS];
};

/**
 * Takes the words each row of the cursor touches from the composition buffer, with the cursor drawn over them, for
 * copying in their place. The cursor never goes into the composition buffer, which in fullscreen is the window's own.
 */
static void __ws_cursor_rows(struct windowserver* ws, struct ws_cursor_row cursor[VESA_CURSOR_HEIGHT])
{
    uint32_t* source = (uint32_t*)ws->_wm->composition_buffer;
    int pitch = vbe_info->pitch;
    int words = ws->_wm->composition_buffer_size / 4;
    int x = ws->m.x, y = ws->m.y;
    int left = MAX(x, 0), right = MIN(x + VESA_CURSOR_WIDTH, vbe_info->width);

    for(int l = 0; l < VESA_CURSOR_HEIGHT; l++){
        struct ws_cursor_row* row = &cursor[l];
        row->first = row->end = 0;
        if(left >= right || y + l < 0 || y + l >= vbe_info->height){
            continue;
        }

        row->first = ((y+l)*pitch + left) / 4;
        row->end = MIN(((y+l)*pitch + right + 3) / 4, words);
        for(int i = row->first; i < row->end; i++){
            row->words[i - row->first] = source[i];
        }
        vesa_put_cursor_row((uint8_t*)row->words + ((y+l)*pitch + left - row->first*4), left, right, x, l);
    }
}

/**
 * Copies rect of the composition buffer to the framebuffer, with the cursor over it, writing only the words that
 * differ from the screen copy. The framebuffer is never read, since reading video memory is slow on a real card
 * (#163). The rect's columns are widened to whole words, so the pixels next to it are written as they are, which is
 * harmless: the composition buffer is up to date everywhere once a frame's redraws are done (#180).
 *
 * Wherever a word the cursor touches is copied, the word with the cursor over it is copied instead. Those are the
 * cursor's own row's words, and when the pitch isn't a multiple of 4, a word at the end of a row can hold the next
 * row's first pixels, so the rows above and below are checked too. Two of the cursor's rows never share a word on a
 * screen at least VESA_CURSOR_WIDTH + 4 pixels wide, which every mode is.
 */
static void __ws_copy_rect(struct windowserver* ws, struct gfx_rect rect, struct ws_cursor_row cursor[VESA_CURSOR_HEIGHT])
{
    uint32_t* framebuffer = (uint32_t*)vbe_info->framebuffer;
    uint32_t* screen = (uint32_t*)ws->_screen;
    uint32_t* source = (uint32_t*)ws->_wm->composition_buffer;
    int pitch = vbe_info->pitch;

    /* VBE_SIZE() is the screen and a byte more, so this is every pixel when the screen's size is a multiple of 4 */
    int words = ws->_wm->composition_buffer_size / 4;

    rect.x0 = MAX(rect.x0, 0);
    rect.y0 = MAX(rect.y0, 0);
    rect.x1 = MIN(rect.x1, vbe_info->width);
    rect.y1 = MIN(rect.y1, vbe_info->height);
    if(rect.x1 <= rect.x0){
        return;
    }

    for(int y = rect.y0; y < rect.y1; y++){
        int next = (y*pitch + rect.x0) / 4;
        int last = MIN((y*pitch + rect.x1 + 3) / 4, words);

        for(int l = y - 1 - ws->m.y; l <= y + 1 - ws->m.y; l++){
            if(l < 0 || l >= VESA_CURSOR_HEIGHT){
                continue;
            }
            int first = MAX(cursor[l].first, next);
            int end = MIN(cursor[l].end, last);
            if(first >= end){
                continue;
            }
            __ws_copy_words(framebuffer+next, screen+next, source+next, first-next);
            __ws_copy_words(framebuffer+first, screen+first, cursor[l].words + (first - cursor[l].first), end-first);
            next = end;
        }
        __ws_copy_words(framebuffer+next, screen+next, source+next, last-next);
    }
}

/* The rectangle around both a and b */
static struct gfx_rect __ws_rect_union(struct gfx_rect a, struct gfx_rect b)
{
    return (struct gfx_rect){ MIN(a.x0, b.x0), MIN(a.y0, b.y0), MAX(a.x1, b.x1), MAX(a.y1, b.y1) };
}

/**
 * Adds rect, clipped to the screen, to a list of up to WS_DAMAGE_MAX parts of the screen to redraw (#180). One that
 * overlaps or touches another is merged with it into the rectangle around both, and when the list is full, they're all
 * merged into one.
 */
static void __ws_damage(struct gfx_rect* list, int* count, struct gfx_rect rect)
{
    rect.x0 = MAX(rect.x0, 0);
    rect.y0 = MAX(rect.y0, 0);
    rect.x1 = MIN(rect.x1, vbe_info->width);
    rect.y1 = MIN(rect.y1, vbe_info->height);
    if(rect.x1 <= rect.x0 || rect.y1 <= rect.y0){
        return;
    }

    /* A merged rectangle can reach ones already passed, so start over after each merge */
    for(int i = 0; i < *count; i++){
        struct gfx_rect other = list[i];
        if(rect.x0 <= other.x1 && other.x0 <= rect.x1 && rect.y0 <= other.y1 && other.y0 <= rect.y1){
            rect = __ws_rect_union(rect, other);
            list[i] = list[--(*count)];
            i = -1;
        }
    }

    if(*count == WS_DAMAGE_MAX){
        for(int i = 0; i < *count; i++){
            rect = __ws_rect_union(rect, list[i]);
        }
        *count = 0;
    }
    list[(*count)++] = rect;
}

/**
 * Copies the redrawn rectangles to the framebuffer, and if the cursor moved, the rectangle it left and the one it's on
 * now, then records where the copy put the cursor (#180).
 */
static void __ws_copy_damage(struct windowserver* ws, struct gfx_rect* damage, int damaged, bool_t cursor_moved)
{
    /* Most frames have nothing to copy, and the window server runs them back to back */
    if(damaged == 0 && !cursor_moved){
        return;
    }

    struct ws_cursor_row cursor[VESA_CURSOR_HEIGHT];
    __ws_cursor_rows(ws, cursor);

    for(int i = 0; i < damaged; i++){
        __ws_copy_rect(ws, damage[i], cursor);
    }
    if(cursor_moved){
        int x = ws->_screen_cursor_x, y = ws->_screen_cursor_y;
        __ws_copy_rect(ws, (struct gfx_rect){ x, y, x + VESA_CURSOR_WIDTH, y + VESA_CURSOR_HEIGHT }, cursor);
        x = ws->m.x;
        y = ws->m.y;
        __ws_copy_rect(ws, (struct gfx_rect){ x, y, x + VESA_CURSOR_WIDTH, y + VESA_CURSOR_HEIGHT }, cursor);
    }

    ws->_screen_cursor_x = ws->m.x;
    ws->_screen_cursor_y = ws->m.y;
}

/**
 * Composites rect of the screen into buffer: the desktop's color, the workspace's label, then every window that
 * overlaps it, back to front, each clipped to it (#180).
 */
static void __ws_composite(struct windowserver* ws, uint8_t* buffer, struct gfx_rect rect)
{
    for(int y = rect.y0; y < rect.y1 && rect.x1 > rect.x0; y++){
        memset(buffer + y*vbe_info->pitch + rect.x0, ws->background, rect.x1 - rect.x0);
    }

    vesa_set_clip(rect.x0, rect.y0, rect.x1, rect.y1);

    /* write current workspace at the right bottom of screen, numbered from 1 like Alt+1 to Alt+4 */
    vesa_printf(buffer, 8, vbe_info->height -8, 0x1F, "Workspace %d", ws->workspace + 1);

    ws->_wm->ops->draw(ws->_wm, buffer, ws->_wm->windows);

    vesa_reset_clip();
}

/* How long screencheck waits for a frame that leaves nothing to redraw */
#define WS_CHECK_TIMEOUT_MS 2000

/**
 * Does a screencheck asked for with ws_check_screen, if there is one: composites the whole screen into its buffer,
 * draws the cursor over it where the copy put it, and compares that with the screen copy. Called at the end of a
 * frame, with interrupts off so nothing draws meanwhile, and only once nothing is left to redraw. Otherwise a later
 * frame does it.
 */
static void __ws_check_screen(struct windowserver* ws)
{
    ENTER_CRITICAL();
    uint8_t* buffer = ws->_check.buffer;
    if(buffer == NULL || ws->_check.done){
        LEAVE_CRITICAL();
        return;
    }

    /* The window draws into the composition buffer itself, so there's nothing to compare it with */
    if(ws->_fullscreen != NULL){
        ws->_check.result = -ERROR_INVALID_ARGUMENTS;
        ws->_check.done = 1;
        LEAVE_CRITICAL();
        return;
    }

    bool_t pending = ws->window_changes;
    for(struct window* w = ws->_wm->windows; w != NULL && !pending; w = w->next){
        pending = w->changed;
    }
    if(pending){
        LEAVE_CRITICAL();
        return;
    }

    int width = vbe_info->width, height = vbe_info->height, pitch = vbe_info->pitch;
    __ws_composite(ws, buffer, (struct gfx_rect){ 0, 0, width, height });

    int x = ws->_screen_cursor_x, y = ws->_screen_cursor_y;
    int left = MAX(x, 0), right = MIN(x + VESA_CURSOR_WIDTH, width);
    for(int l = 0; l < VESA_CURSOR_HEIGHT && left < right; l++){
        if(y + l >= 0 && y + l < height){
            vesa_put_cursor_row(buffer + (y+l)*pitch + left, left, right, x, l);
        }
    }

    int differ = 0;
    struct gfx_rect diff = { width, height, 0, 0 };
    for(int row = 0; row < height; row++){
        for(int column = 0; column < width; column++){
            if(buffer[row*pitch + column] != ws->_screen[row*pitch + column]){
                differ++;
                diff.x0 = MIN(diff.x0, column);
                diff.y0 = MIN(diff.y0, row);
                diff.x1 = MAX(diff.x1, column + 1);
                diff.y1 = MAX(diff.y1, row + 1);
            }
        }
    }

    ws->_check.result = differ;
    ws->_check.diff = diff;
    ws->_check.done = 1;
    LEAVE_CRITICAL();
}

/**
 * Checks that the screen shows what compositing it from scratch gives, for testing what the window server redraws
 * (#180). Returns how many pixels differ, with the rectangle around them in diff, or a negative error: no memory for
 * a second screen, a window fullscreen, another check under way, or no frame without something left to redraw
 * within WS_CHECK_TIMEOUT_MS. A kernel thread that has drawn but not yet committed shows up as a difference.
 */
int ws_check_screen(struct windowserver* ws, struct gfx_rect* diff)
{
    ERR_ON_NULL(ws);
    ERR_ON_NULL(diff);

    /* From the kernel heap, on the caller's thread, since spin_lock can't be taken with interrupts off */
    uint8_t* buffer = kalloc_try(ws->_wm->composition_buffer_size);
    if(buffer == NULL){
        return -ERROR_ALLOC;
    }

    ENTER_CRITICAL();
    if(ws->_check.buffer != NULL){
        LEAVE_CRITICAL();
        kfree(buffer);
        return -ERROR_TIMEOUT;
    }
    ws->_check.done = 0;
    ws->_check.buffer = buffer;
    LEAVE_CRITICAL();

    for(int waited = 0; !ws->_check.done && waited < WS_CHECK_TIMEOUT_MS; waited += 10){
        kernel_sleep(10);
    }

    /* The check runs with interrupts off, so once this runs it's either done or not started, and won't start now */
    ENTER_CRITICAL();
    int done = ws->_check.done;
    ws->_check.buffer = NULL;
    LEAVE_CRITICAL();
    kfree(buffer);

    if(!done){
        return -ERROR_TIMEOUT;
    }
    *diff = ws->_check.diff;
    return ws->_check.result;
}

/**
 * Whether the next frame has something to do, so the window server shouldn't wait for it: input, a cursor that isn't
 * where it's drawn, or something to redraw. In fullscreen only the fullscreen window is drawn, and it stays changed
 * until its owner finishes its frame, so the window server keeps yielding meanwhile. Called with interrupts off.
 */
static bool_t __ws_pending(struct windowserver* ws)
{
    if(mouse_has_event() || kb_has_key()){
        return true;
    }
    if(ws->m.x != ws->_screen_cursor_x || ws->m.y != ws->_screen_cursor_y){
        return true;
    }

    if(ws->_fullscreen != NULL){
        return ws->_fullscreen->changed;
    }

    if(ws->window_changes || ws->_damage_count > 0){
        return true;
    }
    for(struct window* w = ws->_wm->windows; w != NULL; w = w->next){
        if(w->changed){
            return true;
        }
    }
    return false;
}

static int ws_draw(struct windowserver* ws)
{
    ERR_ON_NULL(ws);
    WS_VALIDATE(ws);

    /* Before anything in this frame takes a window pointer */
    __ws_free_windows(ws);
    
    /* get state variables */
    int mouse_changed = mouse_get_event(&ws->m);
    /* Every key typed since the last frame, since frames can be WS_WAIT_MS apart (#199) */
    struct kb_key key;
    while(kb_get_key(&key)){
        __ws_key_event(ws, key.key, key.modifiers);
    }

    /* Read after the keys, since F10 can change it in this frame */
    struct window* fullscreen = ws->_fullscreen;

    /**
     * Redraw only what changed (#180): where each window marked changed was drawn last and where it is now, and where
     * closed windows were, or the whole screen when something asked for that, like leaving fullscreen. The cursor isn't
     * part of it, so moving the mouse alone redraws nothing. In fullscreen the window draws into the composition buffer
     * itself, and nothing clears the other windows' changed, so they wait until fullscreen ends.
     */
    struct gfx_rect damage[WS_DAMAGE_MAX];
    int damaged = 0;
    if(fullscreen == NULL){
        /**
         * Collected with interrupts off: ws_remove adds to it and reads windows' drawn, and other threads ask for whole
         * redraws and move windows. changed is cleared here, before drawing, so a commit made while the window is
         * drawn is drawn in the next frame. drawn is the extent read here, so a move that lands mid-draw leaves the
         * window marked, and the next frame redraws both where it was meant to be and where it went.
         */
        ENTER_CRITICAL();
        int requested = ws->window_changes;
        ws->window_changes = 0;
        for(int i = 0; i < ws->_damage_count; i++){
            damage[i] = ws->_damage[i];
        }
        damaged = ws->_damage_count;
        ws->_damage_count = 0;

        for(struct window* w = ws->_wm->windows; w != NULL; w = w->next){
            if(!w->changed && !requested){
                continue;
            }
            struct gfx_rect extent = gfx_window_extent(w);
            if(w->changed){
                __ws_damage(damage, &damaged, w->drawn);
                __ws_damage(damage, &damaged, extent);
                w->changed = 0;
            }
            w->drawn = extent;
        }

        if(requested){
            damage[0] = (struct gfx_rect){ 0, 0, vbe_info->width, vbe_info->height };
            damaged = 1;
        }
        LEAVE_CRITICAL();

        for(int i = 0; i < damaged; i++){
            __ws_composite(ws, ws->_wm->composition_buffer, damage[i]);
        }
    }

    /* Move out of this module */
    kernel_yield();

    bool_t cursor_moved = ws->m.x != ws->_screen_cursor_x || ws->m.y != ws->_screen_cursor_y;
    if(fullscreen == NULL){
        /**
         * Outside fullscreen, only this thread writes the composition buffer, the screen copy and the framebuffer,
         * and only this thread makes a window fullscreen, so the copy runs with interrupts on.
         */
        __ws_copy_damage(ws, damage, damaged, cursor_moved);
    } else {
        ENTER_CRITICAL();
        /* Read again: the window may have been removed during the yield above, and then the next frame composites */
        struct window* now = ws->_fullscreen;
        if(now != NULL && (now->changed || cursor_moved)){
            /**
             * A fullscreen window draws into the composition buffer itself, so copy it only once the frame is done.
             * Copying mid-draw would put half-drawn frames on screen, making a selection drag or a full redraw flicker.
             *
             * A terminal draws a whole frame in one commit, with drawing set meanwhile, so it's copied as soon as
             * changed is set. That's needed when a program draws into a shell's terminal while the shell is running,
             * waiting for it in pcb_await. Any other window is copied once its owner waits for input or sleeps.
             *
             * Interrupts stay off for the copy, since the frame is only done while the owner can't run: otherwise it
             * could start its next frame partway through. The cursor goes over the copy, not into the window (#165).
             */
            bool_t ready = now->frames_atomic ? !now->drawing
                                              : (now->owner == NULL || now->owner->state != RUNNING);
            if(ready){
                now->changed = 0;
                struct gfx_rect screen = { 0, 0, vbe_info->width, vbe_info->height };
                __ws_copy_damage(ws, &screen, 1, cursor_moved);
            }
        }
        LEAVE_CRITICAL();
    }

    __ws_check_screen(ws);

    /* Every state the mouse driver queued, in order, so a click that came and went within this frame isn't lost */
    while(mouse_changed){
        /* internal mouse event for windows, at the cursor's arrow tip */
        int x = ws->m.x+VESA_CURSOR_HOTSPOT_X;
        int y = ws->m.y+VESA_CURSOR_HOTSPOT_Y;
        if(fullscreen != NULL){
            wm_fullscreen_mouse_event(ws->_wm, fullscreen, x, y, ws->m.flags);
        } else {
            ws->_wm->ops->mouse_event(ws->_wm, x, y, ws->m.flags);
        }
        mouse_changed = mouse_get_event(&ws->m);
    }

    /**
     * Wait for something to draw rather than running frames back to back (#199). Input and drawing threads' commits
     * wake the window server (gfx_composition_wake). Anything else that marks a window changed, like the window manager
     * or a move, is drawn within WS_WAIT_MS. Checked with interrupts off, so a wake can't come between the two.
     */
    ENTER_CRITICAL();
    if(!__ws_pending(ws)){
        kernel_wait(WS_WAIT_MS);
    }
    LEAVE_CRITICAL();

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

    kfree(ws->_screen);
    ws->_wm->ops->destroy(ws->_wm);

    kfree(ws);
    return ERROR_OK;
}
