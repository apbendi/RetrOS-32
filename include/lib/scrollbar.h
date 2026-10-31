#ifndef __LIB_SCROLLBAR_H
#define __LIB_SCROLLBAR_H

/**
 * A vertical scroll bar, for a view that shows `visible` of `total` rows, or of any unit its app picks, from `first`:
 * an arrow button at each end, and between them a track with a thumb, sized to the share that shows. Apps draw it
 * into their windows with libgraphic, from C or C++ (utils/ScrollBar.hpp wraps it).
 *
 * The app owns the struct; nothing is allocated. It places the bar, tells it the numbers when they change, passes it
 * presses, moves and releases, and redraws its view when the bar scrolls it:
 * - an arrow scrolls a row;
 * - a press on the track above or below the thumb scrolls a page, keeping a row of the last one;
 * - dragging the thumb keeps the point pressed under the mouse.
 * The bar is only shown, drawn and pressed, while there are more rows than fit, and only when it's tall enough for its
 * two arrows and the shortest thumb: 3 * SCROLLBAR_WIDTH.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <gfx/events.h>

/* The bar's width, which is also the side of its square arrow buttons and the thumb's shortest */
#define SCROLLBAR_WIDTH 12

/* The parts of the bar, from the top */
enum scrollbar_part {
    SCROLLBAR_NONE,
    SCROLLBAR_UP,
    SCROLLBAR_PAGE_UP,
    SCROLLBAR_THUMB,
    SCROLLBAR_PAGE_DOWN,
    SCROLLBAR_DOWN
};

/* What scrollbar_mouse did with an event, as flags */
/* The event was the bar's: a press on it, or a move or release after one */
#define SCROLLBAR_TAKEN (1 << 0)
/* first changed: redraw the view, and the bar */
#define SCROLLBAR_SCROLLED (1 << 1)
/* Only the bar looks different, an arrow pressed or let go: redraw the bar */
#define SCROLLBAR_REDRAW (1 << 2)

struct scrollbar {
    /* Where it's drawn in the window's content: SCROLLBAR_WIDTH wide and height tall */
    int x, y, height;
    int total, visible, first;
    /* The part a press started on, until its release, and whether the mouse is still over it: an arrow is drawn
     * pressed meanwhile */
    int pressed;
    int over;
    /* On the thumb, where it was pressed, from the thumb's top */
    int grab;
};

void scrollbar_init(struct scrollbar* bar);
void scrollbar_place(struct scrollbar* bar, int x, int y, int height);
/**
 * Sets the numbers, keeping first between 0 and the last row that leaves no room below it.
 * @return whether first changed
 */
int scrollbar_set(struct scrollbar* bar, int total, int visible, int first);
/* Whether the bar is shown: there are more rows than fit, and it's tall enough for its arrows and a thumb */
int scrollbar_shown(const struct scrollbar* bar);
/* The largest first: the one that shows the last row at the bottom */
int scrollbar_max_first(const struct scrollbar* bar);
/* The part at x, y in the window's content, or SCROLLBAR_NONE, as it is when the bar isn't shown */
enum scrollbar_part scrollbar_part_at(const struct scrollbar* bar, int x, int y);
/* Draws the bar, if it's shown */
void scrollbar_draw(const struct scrollbar* bar);
/**
 * Takes a GFX_EVENT_MOUSE_DOWN on the bar, and the moves and release after it.
 * @return 0 for an event that isn't the bar's, or SCROLLBAR_TAKEN with SCROLLBAR_SCROLLED or SCROLLBAR_REDRAW
 */
int scrollbar_mouse(struct scrollbar* bar, const struct gfx_event* event);

#ifdef __cplusplus
}
#endif

#endif /* __LIB_SCROLLBAR_H */
