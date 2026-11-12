/* Host stand-in for apps/utils/TreeView.hpp: an empty tree that records what it's asked to show */
#ifndef EDITOR_TEST_TREEVIEW_HPP
#define EDITOR_TEST_TREEVIEW_HPP

#include <gfx/events.h>
#include <utils/Graphics.hpp>

class TreeView {
public:
    TreeView(int, int, int, int) { g_calls.push_back({'V', 0, 0, 0, 0, 0, ""}); }
    void setScrollable(bool) {}
    void setBox(int, int, bool) {}
    int mouse(const struct gfx_event&) { return 0; }
    const char* click(int, int) { return nullptr; }
    void drawTree(Window*, const char* path) { g_calls.push_back({'T', 0, 0, 0, 0, 0, path ? path : ""}); }
    void refresh() {}
    void resize(int width, int height) { g_calls.push_back({'z', 0, 0, width, height, 0, ""}); }
    void reveal(const char* path) { g_calls.push_back({'v', 0, 0, 0, 0, 0, path ? path : ""}); }
};

#endif
