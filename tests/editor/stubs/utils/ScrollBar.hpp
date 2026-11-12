/* Host stand-in for apps/utils/ScrollBar.hpp: records what it's told, and never takes the mouse */
#ifndef EDITOR_TEST_SCROLLBAR_HPP
#define EDITOR_TEST_SCROLLBAR_HPP

#include <gfx/events.h>
#include <utils/Graphics.hpp>

#define SCROLLBAR_WIDTH 12
#define SCROLLBAR_TAKEN (1 << 0)
#define SCROLLBAR_SCROLLED (1 << 1)
#define SCROLLBAR_REDRAW (1 << 2)

class ScrollBar {
public:
    void place(int x, int y, int height) { g_calls.push_back({'p', x, y, 0, height, 0, ""}); }
    bool set(int total, int visible, int first)
    {
        g_calls.push_back({'s', total, visible, first, 0, 0, ""});
        m_first = first;
        return true;
    }
    int first() const { return m_first; }
    void draw() { g_calls.push_back({'d', 0, 0, 0, 0, 0, ""}); }
    int mouse(const struct gfx_event&) { return 0; }

private:
    int m_first = 0;
};

#endif
