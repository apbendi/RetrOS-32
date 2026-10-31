/**
 * @file scrollbar.c
 * @brief A vertical scroll bar that apps draw into their windows (lib/scrollbar.h), part of libgraphic.
 */

#include <lib/scrollbar.h>
#include <lib/graphics.h>
#include <colors.h>

/* Behind the thumb: a step darker than the face, since the only gray lighter than it is the white of most views */
#define SCROLLBAR_TRACK_COLOR (COLOR_BAR_FACE - 1)
/* The arrows' triangles: rows of 1, 3, 5 and 7 pixels */
#define SCROLLBAR_ARROW_ROWS 4
/* The grip on the thumb: three lines this long, two pixels apart, like the Editor's grip on its tree's border */
#define SCROLLBAR_GRIP_LENGTH 5

void scrollbar_init(struct scrollbar* bar)
{
    bar->x = 0;
    bar->y = 0;
    bar->height = 0;
    bar->total = 0;
    bar->visible = 0;
    bar->first = 0;
    bar->pressed = SCROLLBAR_NONE;
    bar->over = 0;
    bar->grab = 0;
}

void scrollbar_place(struct scrollbar* bar, int x, int y, int height)
{
    bar->x = x;
    bar->y = y;
    bar->height = height;
}

int scrollbar_max_first(const struct scrollbar* bar)
{
    int max = bar->total - bar->visible;
    return max > 0 ? max : 0;
}

int scrollbar_shown(const struct scrollbar* bar)
{
    return bar->total > bar->visible && bar->height >= 3 * SCROLLBAR_WIDTH;
}

/* first, kept between 0 and scrollbar_max_first. @return whether it changed */
static int scrollbar_scroll_to(struct scrollbar* bar, int first)
{
    int max = scrollbar_max_first(bar);
    if (first > max) {
        first = max;
    }
    if (first < 0) {
        first = 0;
    }

    int changed = first != bar->first;
    bar->first = first;
    return changed;
}

int scrollbar_set(struct scrollbar* bar, int total, int visible, int first)
{
    bar->total = total;
    bar->visible = visible;
    return scrollbar_scroll_to(bar, first);
}

/* The track: between the arrows, from just below the top one */
static int scrollbar_track_top(const struct scrollbar* bar)
{
    return bar->y + SCROLLBAR_WIDTH;
}

static int scrollbar_track_length(const struct scrollbar* bar)
{
    int length = bar->height - (2 * SCROLLBAR_WIDTH);
    return length > 0 ? length : 0;
}

/* The share of the track the visible rows are, but at least SCROLLBAR_WIDTH, as far as the track allows */
static int scrollbar_thumb_length(const struct scrollbar* bar)
{
    int track = scrollbar_track_length(bar);
    if (bar->total <= 0) {
        return track;
    }

    int length = track * bar->visible / bar->total;
    if (length < SCROLLBAR_WIDTH) {
        length = SCROLLBAR_WIDTH;
    }
    if (length > track) {
        length = track;
    }
    return length;
}

/* The thumb's top, from the track's: as far down the room below it as first is towards its largest */
static int scrollbar_thumb_offset(const struct scrollbar* bar)
{
    int room = scrollbar_track_length(bar) - scrollbar_thumb_length(bar);
    int max = scrollbar_max_first(bar);
    if (room <= 0 || max <= 0) {
        return 0;
    }
    return room * bar->first / max;
}

enum scrollbar_part scrollbar_part_at(const struct scrollbar* bar, int x, int y)
{
    if (!scrollbar_shown(bar)) {
        return SCROLLBAR_NONE;
    }
    if (x < bar->x || x >= bar->x + SCROLLBAR_WIDTH || y < bar->y || y >= bar->y + bar->height) {
        return SCROLLBAR_NONE;
    }

    int track_top = scrollbar_track_top(bar);
    if (y < track_top) {
        return SCROLLBAR_UP;
    }
    if (y >= track_top + scrollbar_track_length(bar)) {
        return SCROLLBAR_DOWN;
    }

    int thumb_top = track_top + scrollbar_thumb_offset(bar);
    if (y < thumb_top) {
        return SCROLLBAR_PAGE_UP;
    }
    if (y < thumb_top + scrollbar_thumb_length(bar)) {
        return SCROLLBAR_THUMB;
    }
    return SCROLLBAR_PAGE_DOWN;
}

/**
 * An arrow or the thumb: libgraphic's bevel, but raised with a face-colored outer edge at the top and left and the
 * light one inside it, as Windows 95 drew scroll bars, so that edge shows against a white view.
 */
static void scrollbar_draw_part(int x, int y, int height, int pressed)
{
    gfx_draw_bevel(x, y, SCROLLBAR_WIDTH, height, pressed);
    if (pressed) {
        return;
    }

    gfx_draw_rectangle(x, y, SCROLLBAR_WIDTH - 1, 1, COLOR_BAR_FACE);
    gfx_draw_rectangle(x, y, 1, height - 1, COLOR_BAR_FACE);
    gfx_draw_rectangle(x + 1, y + 1, SCROLLBAR_WIDTH - 3, 1, COLOR_BAR_LIGHT);
    gfx_draw_rectangle(x + 1, y + 1, 1, height - 3, COLOR_BAR_LIGHT);
}

/* An arrow button at y, with its triangle pointing up or down, a pixel down and right when pressed */
static void scrollbar_draw_arrow(const struct scrollbar* bar, int y, int up, int pressed)
{
    scrollbar_draw_part(bar->x, y, SCROLLBAR_WIDTH, pressed);

    /* On the face between the two-pixel edges, a pixel left of its middle, since a 7-pixel row can't be in it */
    int shift = pressed ? 1 : 0;
    int center = bar->x + ((SCROLLBAR_WIDTH - 2) / 2) + shift;
    int top = y + ((SCROLLBAR_WIDTH - SCROLLBAR_ARROW_ROWS) / 2) + shift;
    for (int row = 0; row < SCROLLBAR_ARROW_ROWS; row++) {
        int half = up ? row : SCROLLBAR_ARROW_ROWS - 1 - row;
        gfx_draw_rectangle(center - half, top + row, (half * 2) + 1, 1, COLOR_BLACK);
    }
}

void scrollbar_draw(const struct scrollbar* bar)
{
    if (!scrollbar_shown(bar)) {
        return;
    }

    scrollbar_draw_arrow(bar, bar->y, 1, bar->over && bar->pressed == SCROLLBAR_UP);
    scrollbar_draw_arrow(bar, bar->y + bar->height - SCROLLBAR_WIDTH, 0, bar->over && bar->pressed == SCROLLBAR_DOWN);

    int track_top = scrollbar_track_top(bar);
    gfx_draw_rectangle(bar->x, track_top, SCROLLBAR_WIDTH, scrollbar_track_length(bar), SCROLLBAR_TRACK_COLOR);

    int thumb_top = track_top + scrollbar_thumb_offset(bar);
    int thumb = scrollbar_thumb_length(bar);
    scrollbar_draw_part(bar->x, thumb_top, thumb, 0);

    /* The grip, in the middle of the face. The thumb is at least SCROLLBAR_WIDTH tall, so its 5 rows fit */
    int left = bar->x + ((SCROLLBAR_WIDTH - 2 - SCROLLBAR_GRIP_LENGTH) / 2) + 1;
    int middle = thumb_top + ((thumb - 2) / 2);
    for (int line = -1; line <= 1; line++) {
        gfx_draw_rectangle(left, middle + (line * 2), SCROLLBAR_GRIP_LENGTH, 1, COLOR_BAR_SHADOW);
    }
}

int scrollbar_mouse(struct scrollbar* bar, const struct gfx_event* event)
{
    int x = (int16_t)event->data;
    int y = (int16_t)event->data2;

    if (event->event == GFX_EVENT_MOUSE_DOWN) {
        enum scrollbar_part part = scrollbar_part_at(bar, x, y);
        if (part == SCROLLBAR_NONE) {
            return 0;
        }

        bar->pressed = part;
        bar->over = 1;

        /* A page keeps a row of the last one, so where the eye was stays in view */
        int page = bar->visible > 1 ? bar->visible - 1 : 1;
        int target = bar->first;
        switch (part) {
        case SCROLLBAR_UP:
            target -= 1;
            break;
        case SCROLLBAR_DOWN:
            target += 1;
            break;
        case SCROLLBAR_PAGE_UP:
            target -= page;
            break;
        case SCROLLBAR_PAGE_DOWN:
            target += page;
            break;
        default:
            bar->grab = y - (scrollbar_track_top(bar) + scrollbar_thumb_offset(bar));
            break;
        }

        int result = SCROLLBAR_TAKEN | SCROLLBAR_REDRAW;
        if (scrollbar_scroll_to(bar, target)) {
            result |= SCROLLBAR_SCROLLED;
        }
        return result;
    }

    if (bar->pressed == SCROLLBAR_NONE) {
        return 0;
    }
    if (event->event != GFX_EVENT_MOUSE_MOVE && event->event != GFX_EVENT_MOUSE_UP) {
        return 0;
    }

    int result = SCROLLBAR_TAKEN;
    if (bar->pressed == SCROLLBAR_THUMB) {
        /* The point pressed stays under the mouse, as far as the track goes */
        int room = scrollbar_track_length(bar) - scrollbar_thumb_length(bar);
        if (room > 0) {
            int offset = y - bar->grab - scrollbar_track_top(bar);
            if (offset < 0) {
                offset = 0;
            }
            if (offset > room) {
                offset = room;
            }
            int target = ((offset * scrollbar_max_first(bar)) + (room / 2)) / room;
            if (scrollbar_scroll_to(bar, target)) {
                result |= SCROLLBAR_SCROLLED;
            }
        }
    } else {
        /* An arrow is drawn pressed only while the mouse is over it */
        int over = (int)scrollbar_part_at(bar, x, y) == bar->pressed;
        if (over != bar->over) {
            bar->over = over;
            result |= SCROLLBAR_REDRAW;
        }
    }

    if (event->event == GFX_EVENT_MOUSE_UP) {
        bar->pressed = SCROLLBAR_NONE;
        bar->over = 0;
        result |= SCROLLBAR_REDRAW;
    }
    return result;
}
