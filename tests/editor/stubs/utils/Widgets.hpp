/* Host stand-in for apps/utils/Widgets.hpp: do-nothing widgets for the Save As popup's thread, which never starts here */
#ifndef EDITOR_TEST_WIDGETS_HPP
#define EDITOR_TEST_WIDGETS_HPP
class Window;
template <typename T> class Function;
template <typename R, typename... A> class Function<R(A...)> {
public:
    template <typename F> Function(F) {}
};
enum { LEFT, RIGHT, HORIZONTAL, LAYOUT_FLAG_NONE };
class Widget {
public:
    virtual ~Widget() {}
};
class Input : public Widget {
public:
    Input(int, int, const char*, const char*) {}
    void Keyboard(int) {}
    const char* getData() { return ""; }
};
class Button : public Widget {
public:
    Button(int, int, const char*, Function<void()>) {}
};
class Layout {
public:
    Layout(int, int, int, int, int, int) {}
    int addWidget(Widget*, int) { return 0; }
};
class WidgetManager {
public:
    int addLayout(Layout*) { return 0; }
    void draw(Window*) {}
    void Mouse(int, int) {}
};
#endif
