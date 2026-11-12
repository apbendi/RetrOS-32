/**
 * Host stand-in for apps/utils/Graphics.hpp. Everything the Editor draws, through Window or libgraphic, is recorded
 * in g_calls, which the tests read and the trace hashes.
 */
#ifndef EDITOR_TEST_GRAPHICS_HPP
#define EDITOR_TEST_GRAPHICS_HPP

#include <lib/graphics.h>
#include <colors.h>
#include <args.h>
#include <utils/Thread.hpp>
#include <string>
#include <vector>

/**
 * One call: kind is 't' text, 'f' formatted text, 'c' a character, 'r' a rectangle, 'l' a line, 'b' a bevel, 'i' an
 * image, 'e' a frame's end (gfx_end_frame), and from the other stubs, 's' ScrollBar::set (x total, y visible, w first),
 * 'p' ScrollBar::place, 'd' ScrollBar::draw, 'T' TreeView::drawTree (text the path), 'v' TreeView::reveal,
 * 'z' TreeView::resize, 'V' a TreeView made, and 'n' a title. Not drawing, but recorded for where they come among the
 * drawing: 'I' an icon loaded (gfx_load_icon, text its name) and 'R' a file read (x the descriptor, w what it read)
 */
struct DrawCall {
    char kind;
    int x, y, w, h, color;
    std::string text;
};
extern std::vector<DrawCall> g_calls;

class Window {
public:
    Window(int width, int height, const char* name, int flags) { (void)width; (void)height; (void)name; (void)flags; }
    void drawRect(int x, int y, int width, int height, unsigned char color) { gfx_draw_rectangle(x, y, width, height, color); }
    void drawText(int x, int y, const char* text, unsigned char color) { gfx_draw_text(x, y, text, color); }
    void drawContouredRect(int x, int y, int width, int height) { g_calls.push_back({'b', x, y, width, height, 0, ""}); }
    void drawContouredBox(int x, int y, int width, int height, color_t color) { g_calls.push_back({'b', x, y, width, height, color, ""}); }
    void drawFormatText(int, int, unsigned char, const char*, ...) {}
    int drawImage(int x, int y, const struct image* image) { g_calls.push_back({'i', x, y, image->width, image->height, 0, ""}); return 0; }
    void setTitle(const char* name) { g_calls.push_back({'n', 0, 0, 0, 0, 0, name}); }
    void setHeader(const char* header) { g_calls.push_back({'n', 1, 0, 0, 0, 0, header}); }
    void endFrame() { gfx_end_frame(); }
};

#endif
