/**
 * What the Editor's host tests, trace and benchmark share: the stubs' globals, and helpers that drive the Editor the
 * way a user does, through Open and EditorLoop. The helpers are templates, so this compiles without the Editor too.
 */
#ifndef EDITOR_TEST_HARNESS_HPP
#define EDITOR_TEST_HARNESS_HPP

#include <utils/Graphics.hpp>
#include <lib/syscall.h>
#include <gfx/events.h>
#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

/* An entry in g_events for a moment with nothing to read (gfx_get_event in host.cpp) */
#define EDITOR_TEST_NO_EVENT 0xFF

/* The events gfx_get_event gives, in order. With none left, it gives GFX_EVENT_EXIT */
extern std::deque<struct gfx_event> g_events;
/* Called at the start of every event read: after the Editor has handled the last event, before it gets the next */
extern void (*g_beforeEventRead)();
/* The in-memory file system, by absolute path */
extern std::map<std::string, std::string> g_files;
void clipboardReset();

static inline struct gfx_event testEvent(int type, int data = 0, int data2 = 0, int flags = 0)
{
    struct gfx_event event;
    event.event = (uint8_t)type;
    event.flags = (uint8_t)flags;
    event.data = (uint16_t)data;
    event.data2 = (uint16_t)data2;
    return event;
}

/* Opens text in the Editor as the file at path, through Open, as a user would. Unsaved changes are discarded */
template <typename E> bool openText(E* e, const std::string& text, const char* path = "/test.c")
{
    g_files[path] = text;
    static char name[FS_PATH_MAX];
    snprintf(name, sizeof(name), "%s", path);
    return e->Open(name);
}

/* Runs EditorLoop over g_events, then the close after them. @return whether it exited, as the close should make it */
template <typename E> bool runLoop(E* e)
{
    try {
        e->EditorLoop();
    } catch (EditorExit&) {
        return true;
    }
    return false;
}

#endif
