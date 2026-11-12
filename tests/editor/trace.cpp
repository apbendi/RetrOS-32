/**
 * A trace of scripted sessions in the Editor, to check that a change to how it works inside leaves what it does as it
 * was. run.sh builds this with two versions of edit.cpp, runs both, and compares their output.
 * - Sessions: each opens a text through Open, then plays random events through EditorLoop: typing, deleting, the
 *   movement keys with and without Shift, cut, copy, paste, undo and redo, Ctrl+G, Ctrl+F, F2 and F3, presses, drags
 *   and clicks, drags held past the text, resizes, collapsing and dragging the tree, and saving. The events come from
 *   a fixed seed, never from the Editor's state, so both versions get the same ones.
 * - Output: a line per event, after the Editor has handled it, with the cursor, selection and view, a hash of the
 *   text, and the number and a hash of the draw calls it made. Each session ends with a hash of the file it saved.
 * - --dump SESSION:EVENT prints that event's draw calls in full, to see how two versions differ.
 * - --ignore-char-colors leaves the colors characters are drawn in out of the hash, for a change to coloring alone.
 */
#define private public
#define main editor_main
#include EDIT_CPP
#undef main

#include "harness.hpp"

#include <random>

static uint64_t fnv(uint64_t hash, const void* data, size_t size)
{
    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 1099511628211ULL;
    }
    return hash;
}

static uint64_t fnvInt(uint64_t hash, int value)
{
    return fnv(hash, &value, sizeof(value));
}

static const uint64_t FNV_START = 14695981039346656037ULL;

/* The run's state, for the callback that writes a line at each event read */
static Editor* g_editor = nullptr;
static int g_session = 0;
static int g_event = 0;
static std::string g_handling;
static std::string g_dump;
static bool g_ignoreCharColors = false;

static std::string describe(const struct gfx_event& e)
{
    char text[64];
    int16_t x = (int16_t)e.data;
    int16_t y = (int16_t)e.data2;
    switch (e.event) {
    case GFX_EVENT_KEYBOARD:
        snprintf(text, sizeof(text), "key:%d/%d", e.data, e.flags);
        break;
    case GFX_EVENT_MOUSE_DOWN:
        snprintf(text, sizeof(text), "down:%d,%d/%d", x, y, e.flags);
        break;
    case GFX_EVENT_MOUSE_MOVE:
        snprintf(text, sizeof(text), "move:%d,%d", x, y);
        break;
    case GFX_EVENT_MOUSE_UP:
        snprintf(text, sizeof(text), "up:%d,%d", x, y);
        break;
    case GFX_EVENT_MOUSE:
        snprintf(text, sizeof(text), "click:%d,%d", x, y);
        break;
    case GFX_EVENT_RESOLUTION:
        snprintf(text, sizeof(text), "size:%dx%d", e.data, e.data2);
        break;
    case EDITOR_TEST_NO_EVENT:
        snprintf(text, sizeof(text), "tick");
        break;
    default:
        snprintf(text, sizeof(text), "event:%d", e.event);
        break;
    }
    return text;
}

/* A line for what the Editor did with the last event, from the draw calls since, which it then forgets */
static void emit()
{
    Editor* e = g_editor;
    char id[32];
    snprintf(id, sizeof(id), "%d:%d", g_session, g_event);

    if (!g_dump.empty()) {
        if (g_dump == id) {
            for (const DrawCall& c : g_calls) {
                printf("%c %d %d %d %d %d %s\n", c.kind, c.x, c.y, c.w, c.h, c.color, c.text.c_str());
            }
        }
    } else {
        uint64_t draw = FNV_START;
        for (const DrawCall& c : g_calls) {
            draw = fnv(draw, &c.kind, 1);
            int color = g_ignoreCharColors && c.kind == 'c' ? 0 : c.color;
            draw = fnvInt(fnvInt(fnvInt(fnvInt(fnvInt(draw, c.x), c.y), c.w), c.h), color);
            draw = fnv(draw, c.text.data(), c.text.size());
        }
        uint64_t text = fnv(FNV_START, e->m_textBuffer, e->m_bufferHead);
        printf("%s %s cur=%d sel=%d sy=%d wrap=%d len=%d text=%016llx calls=%d draw=%016llx\n", id, g_handling.c_str(),
               e->m_bufferEdit, e->m_selAnchor, e->scrollY, e->m_scrollWrap, e->m_bufferHead, (unsigned long long)text,
               (int)g_calls.size(), (unsigned long long)draw);
    }

    g_calls.clear();
    g_event++;
}

static void beforeEventRead()
{
    emit();
    g_handling = g_events.empty() ? "close" : describe(g_events.front());
}

/* rng() % n, which unlike the standard distributions gives the same numbers with every C++ library */
struct Rng {
    std::mt19937 gen;
    explicit Rng(uint32_t seed) : gen(seed) {}
    int below(int n) { return n > 0 ? (int)(gen() % (uint32_t)n) : 0; }
    bool chance(int percent) { return below(100) < percent; }
};

enum { TEXT_CODE, TEXT_SHORT_LINES, TEXT_ONE_LINE, TEXT_EMPTY };

/* Text like a C file: indented lines with tabs or spaces, braces, the odd long line, and empty lines */
static std::string makeText(Rng& rng, int kind, int size)
{
    static const char* words[] = {"int", "char", "return", "if", "else", "while", "for", "x", "count", "buffer",
                                  "printf(\"%d\\n\", x);", "{", "}", "/* note */", "i++;", "=", "+", "0", "NULL",
                                  "struct", "size_t", "len", "->", "(", ")", "[i]", "// comment", "#include <stdio.h>"};
    std::string text;
    int depth = 0;
    while ((int)text.size() < size) {
        if (kind == TEXT_ONE_LINE) {
            text += words[rng.below(28)];
            text += rng.chance(10) ? "\t" : " ";
            continue;
        }
        if (kind == TEXT_SHORT_LINES) {
            int len = rng.below(6);
            for (int i = 0; i < len; i++) {
                text += (char)('a' + rng.below(26));
            }
            text += '\n';
            continue;
        }

        int indent = rng.chance(5) ? 0 : depth;
        for (int i = 0; i < indent; i++) {
            text += rng.chance(50) ? "\t" : "    ";
        }
        int count = rng.chance(5) ? 25 + rng.below(20) : rng.below(9);
        for (int i = 0; i < count; i++) {
            const char* word = words[rng.below(28)];
            text += word;
            text += ' ';
            depth += word[0] == '{' ? 1 : 0;
            depth -= word[0] == '}' && depth > 0 ? 1 : 0;
            if (depth > 6) {
                depth = 6;
            }
        }
        text += '\n';
    }
    text.resize(size);
    return text;
}

static void key(std::deque<struct gfx_event>& out, int c, int mods = 0)
{
    out.push_back(testEvent(GFX_EVENT_KEYBOARD, c, 0, mods));
}

/* A random session of n events, in a window that starts width by height */
static std::deque<struct gfx_event> makeEvents(Rng& rng, int n, int width, int height)
{
    static const char typed[] = "abcxyz_ 0123{}();=,\"'#*/-+";
    std::deque<struct gfx_event> out;
    out.push_back(testEvent(GFX_EVENT_RESOLUTION, width, height));
    while ((int)out.size() < n) {
        int shift = rng.chance(30) ? KEY_MOD_SHIFT : 0;
        int r = rng.below(100);
        if (r < 25 || r >= 94) {
            key(out, typed[rng.below((int)sizeof(typed) - 1)]);
        } else if (r < 29) {
            key(out, '\n');
        } else if (r < 31) {
            key(out, '\t');
        } else if (r < 37) {
            key(out, '\b');
        } else if (r < 40) {
            key(out, KEY_DELETE);
        } else if (r < 52) {
            static const int arrows[] = {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN};
            key(out, arrows[rng.below(4)], shift);
        } else if (r < 55) {
            key(out, rng.chance(50) ? KEY_HOME : KEY_END, shift);
        } else if (r < 59) {
            key(out, rng.chance(50) ? KEY_PAGE_UP : KEY_PAGE_DOWN, rng.chance(20) ? KEY_MOD_SHIFT : 0);
        } else if (r < 60) {
            key(out, CTRLA);
        } else if (r < 61) {
            key(out, 27);
        } else if (r < 62) {
            key(out, CTRLC, shift);
        } else if (r < 63) {
            key(out, CTRLX);
        } else if (r < 65) {
            key(out, CTRLV);
        } else if (r < 68) {
            key(out, CTRLZ, rng.chance(20) ? KEY_MOD_SHIFT : 0);
        } else if (r < 69) {
            key(out, CTRLY);
        } else if (r < 71) {
            key(out, rng.chance(50) ? KEY_F2 : KEY_F3);
        } else if (r < 73) {
            /* Go to a line, or think better of it */
            key(out, CTRLG);
            int digits = 1 + rng.below(5);
            for (int i = 0; i < digits; i++) {
                key(out, '0' + rng.below(10));
            }
            key(out, rng.chance(80) ? '\n' : 27);
        } else if (r < 76) {
            /* Find something, step through it, then Esc or a click */
            key(out, CTRLF);
            if (rng.chance(70)) {
                key(out, '\b');
            }
            int chars = 1 + rng.below(3);
            for (int i = 0; i < chars; i++) {
                key(out, "ix(;{ \"r"[rng.below(8)]);
            }
            int steps = 1 + rng.below(3);
            for (int i = 0; i < steps; i++) {
                key(out, '\n', rng.chance(30) ? KEY_MOD_SHIFT : 0);
            }
            if (rng.chance(70)) {
                key(out, 27);
            }
        } else if (r < 86) {
            /* A press, maybe a drag, maybe past the text's top or bottom and held there, then the release and the click */
            int x = 60 + rng.below(width > 60 ? width - 60 : 1);
            int y = rng.below(height);
            int clicks = rng.chance(80) ? 1 : 2 + rng.below(2);
            out.push_back(testEvent(GFX_EVENT_MOUSE_DOWN, x, y, (rng.chance(15) ? KEY_MOD_SHIFT : 0) | (clicks << GFX_EVENT_CLICKS_SHIFT)));
            int moves = rng.below(4);
            for (int i = 0; i < moves; i++) {
                x = 60 + rng.below(width > 60 ? width - 60 : 1);
                y = rng.chance(20) ? (rng.chance(50) ? -5 : height + 5) : rng.below(height);
                out.push_back(testEvent(GFX_EVENT_MOUSE_MOVE, (uint16_t)(int16_t)x, (uint16_t)(int16_t)y));
                if (y < 0 || y >= height) {
                    int ticks = 1 + rng.below(3);
                    for (int t = 0; t < ticks; t++) {
                        out.push_back(testEvent(EDITOR_TEST_NO_EVENT));
                    }
                }
            }
            out.push_back(testEvent(GFX_EVENT_MOUSE_UP, (uint16_t)(int16_t)x, (uint16_t)(int16_t)y));
            if (y >= 0 && y < height) {
                out.push_back(testEvent(GFX_EVENT_MOUSE, x, y));
            }
        } else if (r < 88) {
            width = 200 + rng.below(800);
            height = 80 + rng.below(620);
            out.push_back(testEvent(GFX_EVENT_RESOLUTION, width, height));
        } else if (r < 89) {
            key(out, CTRLB);
        } else if (r < 90) {
            /* A drag on the tree's border, which is where it is only while the tree is open and as wide as it started */
            int y = 40 + rng.below(height > 80 ? height - 80 : 1);
            out.push_back(testEvent(GFX_EVENT_MOUSE_DOWN, 128, y, 1 << GFX_EVENT_CLICKS_SHIFT));
            out.push_back(testEvent(GFX_EVENT_MOUSE_MOVE, 100 + rng.below(200), y));
            out.push_back(testEvent(GFX_EVENT_MOUSE_UP, 100 + rng.below(200), y));
        } else if (r < 91) {
            key(out, CTRLS);
        } else {
            key(out, CTRLH);
        }
    }
    return out;
}

struct Session {
    int kind;
    int size;
    int width;
    int height;
    int events;
};

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            g_dump = argv[++i];
        } else if (strcmp(argv[i], "--ignore-char-colors") == 0) {
            g_ignoreCharColors = true;
        }
    }

    /* Texts of each kind, near the 64 KB limit and over it (read-only), in windows from narrow to wide */
    static const Session sessions[] = {
        {TEXT_CODE, 3000, EDITOR_DEFAULT_WINDOW_WIDTH, EDITOR_DEFAULT_HEIGHT, 400},
        {TEXT_CODE, 20000, 900, 600, 300},
        {TEXT_CODE, 60000, 640, 400, 200},
        {TEXT_SHORT_LINES, 60000, 700, 500, 200},
        {TEXT_ONE_LINE, 5000, 400, 300, 300},
        {TEXT_EMPTY, 0, EDITOR_DEFAULT_WINDOW_WIDTH, EDITOR_DEFAULT_HEIGHT, 300},
        {TEXT_CODE, 8000, 260, 160, 300},
        {TEXT_CODE, 70000, 800, 500, 150},
        {TEXT_CODE, 500, 1000, 700, 300},
        {TEXT_SHORT_LINES, 4000, 300, 700, 300},
    };

    for (int s = 0; s < (int)(sizeof(sessions) / sizeof(sessions[0])); s++) {
        const Session& session = sessions[s];
        Rng rng(1000 + s);
        g_session = s;
        g_event = 0;
        g_handling = "open";
        g_files.clear();
        clipboardReset();
        g_calls.clear();

        Editor* e = new Editor();
        g_editor = e;
        std::string text = makeText(rng, session.kind, session.size);
        openText(e, text, "/s.c");
        g_events = makeEvents(rng, session.events, session.width, session.height);

        g_beforeEventRead = beforeEventRead;
        bool exited = runLoop(e);
        g_beforeEventRead = nullptr;
        emit();

        if (g_dump.empty()) {
            const std::string& saved = g_files["/s.c"];
            printf("%d:end exited=%d saved=%d %016llx\n", s, exited ? 1 : 0, (int)saved.size(),
                   (unsigned long long)fnv(FNV_START, saved.data(), saved.size()));
        }
        delete e;
    }
    return 0;
}
