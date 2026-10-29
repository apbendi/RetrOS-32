/**
 * @file composition.c
 * @author Joe Bayer (joexbayer)
 * @brief Window Server composition and window manage api.
 * @version 0.1
 * @date 2023-01-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <gfx/window.h>
#include <gfx/gfxlib.h>
#include <gfx/events.h>
#include <math.h>
#include <gfx/windowserver.h>
#include <windowmanager.h>
#include <kthreads.h>

/* prototypes */
void __kthread_entry gfx_compositor_main();

struct windowserver* ws;

/**
 * @brief Removes a window from the "wind.order" list.
 * Important keep the wind.order list intact even if removing the first element.
 * @param w 
 */
void gfx_composition_remove_window(struct window* w)
{
     while (ws == NULL || ws->_wm->state != WM_INITIALIZED);

     ws->ops->remove(ws, w);
}

/**
 * @brief Adds new window in the "wind.order" list.
 * 
 * @param w 
 */
void gfx_composition_add_window(struct window* w)
{
    dbgprintf("[WSERVER] Adding window to window server.\n");
    if(ws->ops->add(ws, w) < 0){
        dbgprintf("[WSERVER] Could not add window to window server.\n");
    }

}

int gfx_set_background_color(color_t color)
{
    ERR_ON_NULL(ws);

    ws->ops->set_background(ws, color);
    return ERROR_OK;
}

/* Asks the window server to redraw the whole screen, for a change that affects every window, like the theme (#180) */
void gfx_composition_redraw(void)
{
    if(ws != NULL){
        ws->window_changes = 1;
    }
}

int gfx_set_taskbar(pid_t pid)
{
    ERR_ON_NULL(ws);
    struct pcb* taskbar = pcb_get_by_name("taskbar");
    if(taskbar == NULL){
        pid_t pid = start("taskbar", 0, NULL);
        if(pid < 0){
            dbgprintf("[WSERVER] Could not start taskbar.\n");
            return -1;
        }

        taskbar = pcb_get_by_name("taskbar");
    }

    ws->taskbar = taskbar;
    return ERROR_OK;
}

/**
 * @brief Copies the windows on screen, front to back (see wm_get_windows).
 * @return int how many windows there are, or -1 when there's no window server, e.g. in text mode
 */
int gfx_get_windows(struct wm_window_info* out, int max)
{
    if(ws == NULL || ws->_wm == NULL || ws->_wm->state != WM_INITIALIZED){
        return -1;
    }

    return wm_get_windows(ws->_wm, out, max);
}

/**
 * Checks that the screen shows what compositing it from scratch gives (ws_check_screen). Returns how many pixels
 * differ, with the rectangle around them in diff, or a negative error, -ERROR_WINDOW_NOT_FOUND when there's no window
 * server, e.g. in text mode.
 */
int gfx_check_screen(struct gfx_rect* diff)
{
    if(ws == NULL || ws->_wm == NULL || ws->_wm->state != WM_INITIALIZED){
        return -ERROR_WINDOW_NOT_FOUND;
    }

    return ws_check_screen(ws, diff);
}

void __kthread_entry gfx_compositor_main()
{
    ws = ws_new();
    if(ws == NULL){
        dbgprintf("[WSERVER] Could not allocate memory for window server.\n");
        return;
    }

    dbgprintf("[WSERVER] Initializing window server...\n");
    
    //ws->ops->set_background(ws, 3);

    dbgprintf("[WSERVER] Window server initialized.\n");

    start("login", 0, NULL);
    while(1){
        ws->ops->draw(ws);
    }
}