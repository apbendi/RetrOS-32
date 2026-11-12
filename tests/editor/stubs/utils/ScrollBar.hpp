/**
 * Host stand-in for apps/utils/ScrollBar.hpp: records what it's told, never takes the mouse, and keeps its first row
 * and says whether it's shown as lib/scrollbar.c does
 */
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
    void place(int x, int y, int height)
    {
        g_calls.push_back({'p', x, y, 0, height, 0, ""});
        m_x = x;
        m_height = height;
    }
    bool set(int total, int visible, int first)
    {
        g_calls.push_back({'s', total, visible, first, 0, 0, ""});
        m_total = total;
        m_visible = visible;
        int max = total - visible > 0 ? total - visible : 0;
        first = first > max ? max : first;
        first = first < 0 ? 0 : first;
        bool changed = first != m_first;
        m_first = first;
        return changed;
    }
    bool shown() const { return m_total > m_visible && m_height >= 3 * SCROLLBAR_WIDTH; }
    int first() const { return m_first; }
    int x() const { return m_x; }
    void draw() { g_calls.push_back({'d', 0, 0, 0, 0, 0, ""}); }
    int mouse(const struct gfx_event&) { return 0; }

private:
    int m_x = 0;
    int m_height = 0;
    int m_total = 0;
    int m_visible = 0;
    int m_first = 0;
};

#endif
