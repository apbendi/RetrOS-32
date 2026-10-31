/**
 * @file ScrollBar.hpp
 * @brief libgraphic's vertical scroll bar (lib/scrollbar.h), as a class.
 */

#ifndef __SCROLLBAR_HPP__
#define __SCROLLBAR_HPP__

#include <lib/scrollbar.h>
#include <gfx/events.h>

/**
 * A vertical scroll bar for a view that shows `visible` of `total` rows from `first`. Its owner places it, sets the
 * numbers, passes it presses, moves and releases, and redraws the view when it scrolls. See lib/scrollbar.h.
 */
class ScrollBar {
public:
    ScrollBar() {
        scrollbar_init(&m_bar);
    }

    void place(int x, int y, int height) {
        scrollbar_place(&m_bar, x, y, height);
    }

    /* @return whether first changed, which is kept in range */
    bool set(int total, int visible, int first) {
        return scrollbar_set(&m_bar, total, visible, first) != 0;
    }

    /* Whether the bar is shown, drawn and pressed: there are more rows than fit, and it's tall enough for it */
    bool shown() const {
        return scrollbar_shown(&m_bar) != 0;
    }

    int first() const {
        return m_bar.first;
    }

    int x() const {
        return m_bar.x;
    }

    void draw() const {
        scrollbar_draw(&m_bar);
    }

    /* @return 0 for an event that isn't the bar's, or SCROLLBAR_TAKEN with SCROLLBAR_SCROLLED or SCROLLBAR_REDRAW */
    int mouse(const struct gfx_event& event) {
        return scrollbar_mouse(&m_bar, &event);
    }

private:
    struct scrollbar m_bar;
};

#endif /* __SCROLLBAR_HPP__ */
