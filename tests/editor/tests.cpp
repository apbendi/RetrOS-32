/**
 * Host tests of the Editor's prompts (#137): Ctrl+G and Ctrl+F's keys, where goToLine, findNext and revealRange put
 * the cursor, selection and view, against models of each, the prompt drawn at every width, and EditorLoop's handling
 * of a press and a resize while one is open. Built by run.sh around the real apps/editor/edit.cpp.
 */
#define private public
#define main editor_main
#include EDIT_CPP
#undef main

#include "harness.hpp"

#include <random>

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        g_checks++;                                                         \
        if (!(cond)) {                                                      \
            g_failures++;                                                   \
            if (g_failures <= 30) {                                         \
                printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond);      \
                printf(__VA_ARGS__);                                        \
                printf("\n");                                               \
            }                                                               \
        }                                                                   \
    } while (0)

static void setText(Editor* e, const std::string& text)
{
    CHECK(openText(e, text), "couldn't open a text of %d bytes", (int)text.size());
}

static std::string numberedLines(int n, bool trailing)
{
    std::string s;
    for (int i = 1; i <= n; i++) {
        s += "line " + std::to_string(i);
        if (i < n || trailing) {
            s += "\n";
        }
    }
    return s;
}

/* The last status line text drawn: drawStatusLine draws it with gfx_draw_format_text at the field's text row */
static const DrawCall* lastStatusText(Editor* e)
{
    int y = e->c_height - STATUS_HEIGHT + EDITOR_STATUS_MARGIN + 2;
    for (int i = (int)g_calls.size() - 1; i >= 0; i--) {
        if (g_calls[i].kind == 'f' && g_calls[i].y == y && g_calls[i].x == e->textStartX()) {
            return &g_calls[i];
        }
    }
    return nullptr;
}

/* The prompt's cursor drawn after the last status text: an 8x8 rectangle on the status line's text row */
static const DrawCall* promptCursorAfterStatus(Editor* e)
{
    int y = e->c_height - STATUS_HEIGHT + EDITOR_STATUS_MARGIN + 2;
    const DrawCall* found = nullptr;
    for (int i = (int)g_calls.size() - 1; i >= 0; i--) {
        if (g_calls[i].kind == 'f' && g_calls[i].y == y && g_calls[i].x == e->textStartX()) {
            break;
        }
        if (g_calls[i].kind == 'r' && g_calls[i].y == y && g_calls[i].w == 8 && g_calls[i].h == 8) {
            found = &g_calls[i];
        }
    }
    return found;
}

static int lineStartIndex(const std::string& text, int line)
{
    int current = 1;
    for (int i = 0; i < (int)text.size(); i++) {
        if (current == line) {
            return i;
        }
        if (text[i] == '\n') {
            current++;
        }
    }
    return current == line ? (int)text.size() : -1;
}

static int lineCount(const std::string& text)
{
    int n = 1;
    for (char c : text) {
        n += c == '\n';
    }
    return n;
}

/**
 * Each index's row, counting the text's rows from its start, laid out independently of the Editor's scrolling code:
 * an index's row is where the character there is placed, and the text's end is where a newline after it would be
 */
static std::vector<int> rowsOf(const std::string& text, int cols)
{
    std::vector<int> rows(text.size() + 1);
    TextLayout at(cols);
    for (size_t i = 0; i < text.size(); i++) {
        at.place((unsigned char)text[i]);
        rows[i] = at.row;
    }
    at.place('\n');
    rows[text.size()] = at.row;
    return rows;
}

static void key(Editor* e, unsigned char c, int mods = 0)
{
    e->putChar(c, mods);
}

static void testPromptKeys()
{
    Editor* e = new Editor();
    std::string text = numberedLines(300, true);
    setText(e, text);
    e->reDraw(0, e->m_bufferHead);

    /* Ctrl+G opens the prompt, moving nothing */
    e->m_bufferEdit = lineStartIndex(text, 5) + 2;
    g_calls.clear();
    key(e, CTRLG);
    CHECK(e->m_prompt == EDITOR_PROMPT_LINE, "prompt %d", e->m_prompt);
    CHECK(e->m_bufferEdit == lineStartIndex(text, 5) + 2, "cursor moved to %d", e->m_bufferEdit);
    const DrawCall* status = lastStatusText(e);
    CHECK(status != nullptr && status->text == "Go to line (1-301): ", "status '%s'", status ? status->text.c_str() : "(none)");
    CHECK(status != nullptr && status->color == COLOR_BLACK, "color %d", status ? status->color : -1);
    const DrawCall* cursor = promptCursorAfterStatus(e);
    CHECK(cursor != nullptr && cursor->x == e->textStartX() + 20 * 8, "cursor x %d", cursor ? cursor->x : -1);

    /* Digits go in; letters, arrows, Ctrl keys and F-keys do nothing, to the prompt or the text */
    unsigned char ignored[] = {'a', 'Z', ' ', '-', KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_END, KEY_PAGE_UP,
                               KEY_PAGE_DOWN, KEY_DELETE, KEY_F2, KEY_F3, CTRLA, CTRLB, CTRLC, CTRLH, CTRLN, CTRLS,
                               CTRLV, CTRLX, CTRLY, CTRLZ, '\t', 0, 127, 200};
    key(e, '5');
    for (unsigned char c : ignored) {
        key(e, c);
        key(e, c, KEY_MOD_SHIFT);
    }
    key(e, '7');
    CHECK(std::string(e->m_promptText) == "57", "typed '%s'", e->m_promptText);
    CHECK(e->m_bufferHead == (int)text.size() && memcmp(e->m_textBuffer, text.data(), text.size()) == 0, "text changed");
    CHECK(e->m_bufferEdit == lineStartIndex(text, 5) + 2, "cursor moved to %d", e->m_bufferEdit);
    CHECK(!e->m_treeCollapsed, "Ctrl+B reached the Editor");
    CHECK(e->m_selAnchor == -1, "Ctrl+A or Shift reached the Editor");
    status = lastStatusText(e);
    CHECK(status != nullptr && status->text == "Go to line (1-301): 57", "status '%s'", status ? status->text.c_str() : "(none)");

    /* Backspace, and at most five digits */
    key(e, '\b');
    CHECK(std::string(e->m_promptText) == "5", "typed '%s'", e->m_promptText);
    key(e, '\b');
    key(e, '\b');
    CHECK(std::string(e->m_promptText) == "", "typed '%s'", e->m_promptText);
    for (const char* p = "1234567"; *p; p++) {
        key(e, *p);
    }
    CHECK(std::string(e->m_promptText) == "12345", "typed '%s'", e->m_promptText);

    /* Esc closes it, moving nothing, and the file's status line comes back */
    g_calls.clear();
    key(e, 27);
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "prompt %d", e->m_prompt);
    CHECK(e->m_bufferEdit == lineStartIndex(text, 5) + 2, "cursor moved to %d", e->m_bufferEdit);
    status = lastStatusText(e);
    CHECK(status != nullptr && status->text == "/test.c  Ln:5 Col:3", "status '%s'", status ? status->text.c_str() : "(none)");

    /* Ctrl+G again closes it too */
    key(e, CTRLG);
    key(e, '9');
    key(e, CTRLG);
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE && e->m_bufferEdit == lineStartIndex(text, 5) + 2, "Ctrl+G didn't close");

    /* Enter with nothing typed only closes it */
    key(e, CTRLG);
    key(e, '\n');
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE && e->m_bufferEdit == lineStartIndex(text, 5) + 2, "empty Enter moved");

    /* Enter goes to the line, and the status line says so. \r works as Enter */
    struct { const char* typed; int line; unsigned char enter; } cases[] = {
        {"57", 57, '\n'}, {"1", 1, '\r'}, {"0", 1, '\n'}, {"00042", 42, '\n'}, {"301", 301, '\n'},
        {"302", 301, '\n'}, {"99999", 301, '\n'}, {"150", 150, '\r'},
    };
    for (auto& c : cases) {
        key(e, CTRLG);
        for (const char* p = c.typed; *p; p++) {
            key(e, *p);
        }
        g_calls.clear();
        key(e, c.enter);
        CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "prompt open after Enter");
        CHECK(e->m_bufferEdit == lineStartIndex(text, c.line), "'%s': cursor %d, want %d", c.typed, e->m_bufferEdit, lineStartIndex(text, c.line));
        status = lastStatusText(e);
        std::string want = "/test.c  Ln:" + std::to_string(c.line) + " Col:1";
        CHECK(status != nullptr && status->text == want, "'%s': status '%s'", c.typed, status ? status->text.c_str() : "(none)");
    }

    /* A selection stays through the prompt and Esc, and Enter clears it */
    e->m_selAnchor = 10;
    e->m_bufferEdit = 40;
    key(e, CTRLG);
    key(e, '3');
    key(e, 27);
    CHECK(e->m_selAnchor == 10 && e->m_bufferEdit == 40, "Esc changed the selection");
    key(e, CTRLG);
    key(e, '3');
    key(e, '\n');
    CHECK(e->m_selAnchor == -1 && e->m_bufferEdit == lineStartIndex(text, 3), "Enter left the selection");

    /* Typing after a jump is an undo step of its own, and the text was never changed by the prompt */
    e->m_bufferEdit = lineStartIndex(text, 10);
    key(e, 'a');
    key(e, 'b');
    key(e, CTRLG);
    key(e, '2');
    key(e, '0');
    key(e, '\n');
    key(e, 'c');
    e->undo();
    std::string want = text;
    want.insert(lineStartIndex(text, 10), "ab");
    CHECK(e->m_bufferHead == (int)want.size() && memcmp(e->m_textBuffer, want.data(), want.size()) == 0, "undo after a jump undid more than the c");
    e->undo();
    CHECK(e->m_bufferHead == (int)text.size() && memcmp(e->m_textBuffer, text.data(), text.size()) == 0, "second undo didn't restore the text");

    /* A read-only file can be moved around with it, and digits don't count as typing into it */
    setText(e, text);
    e->m_readOnly = true;
    g_calls.clear();
    key(e, CTRLG);
    key(e, '8');
    status = lastStatusText(e);
    CHECK(status != nullptr && status->text == "Go to line (1-301): 8", "read-only status '%s'", status ? status->text.c_str() : "(none)");
    key(e, '\n');
    CHECK(e->m_bufferEdit == lineStartIndex(text, 8), "read-only: cursor %d", e->m_bufferEdit);

    delete e;
}

/* Random keys at the prompt, against a model of what it keeps; the text, cursor, selection and view never change until Enter */
static void testPromptRandom(std::mt19937& rng)
{
    Editor* e = new Editor();
    std::string text = numberedLines(500, false);
    setText(e, text);

    std::vector<unsigned char> keys;
    for (int c = 1; c < 256; c++) {
        if (c != 27 && c != CTRLG && c != '\n' && c != '\r') {
            keys.push_back((unsigned char)c);
        }
    }
    for (int i = 0; i < 40; i++) {
        keys.push_back('\b');
        keys.push_back((unsigned char)('0' + (i % 10)));
    }

    for (int trial = 0; trial < 3000; trial++) {
        e->m_bufferEdit = (int)(rng() % (text.size() + 1));
        e->m_selAnchor = rng() % 2 ? (int)(rng() % (text.size() + 1)) : -1;
        e->scrollY = (int)(rng() % 500);
        e->m_scrollWrap = 0;
        int edit = e->m_bufferEdit;
        int anchor = e->m_selAnchor;
        int scroll_y = e->scrollY;

        key(e, CTRLG);
        std::string model;
        int n = (int)(rng() % 30);
        for (int k = 0; k < n; k++) {
            unsigned char c = keys[rng() % keys.size()];
            key(e, c, (int)(rng() % 8));
            if (c == '\b') {
                if (!model.empty()) {
                    model.pop_back();
                }
            } else if (c >= '0' && c <= '9' && model.size() < 5) {
                model += (char)c;
            }
        }
        CHECK(std::string(e->m_promptText) == model, "trial %d: typed '%s', model '%s'", trial, e->m_promptText, model.c_str());
        CHECK(e->m_bufferHead == (int)text.size() && memcmp(e->m_textBuffer, text.data(), text.size()) == 0, "trial %d: text changed", trial);
        CHECK(e->m_bufferEdit == edit && e->m_selAnchor == anchor && e->scrollY == scroll_y, "trial %d: moved", trial);

        if (rng() % 2) {
            key(e, 27);
            CHECK(e->m_bufferEdit == edit && e->m_selAnchor == anchor, "trial %d: Esc moved", trial);
        } else {
            key(e, '\n');
            if (model.empty()) {
                CHECK(e->m_bufferEdit == edit && e->m_selAnchor == anchor, "trial %d: empty Enter moved", trial);
            } else {
                int line = clampInt(atoi(model.c_str()), 1, 500);
                CHECK(e->m_bufferEdit == lineStartIndex(text, line) && e->m_selAnchor == -1, "trial %d: '%s' went to %d", trial, model.c_str(), e->m_bufferEdit);
            }
        }
        CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "trial %d: prompt still open", trial);
    }
    delete e;
}

/**
 * goToLine on random texts, window sizes and views, against a model: a line whose start is in view leaves the view as
 * it is; otherwise the view's first row is the line's row less half the rows, but no further down than leaves the
 * text's last row at the bottom, and not above the top
 */
static void testGoToLineRandom(std::mt19937& rng, int trials)
{
    Editor* e = new Editor();
    int moved = 0;
    int stayed = 0;
    int centered = 0;
    int at_end = 0;
    int at_top = 0;

    for (int trial = 0; trial < trials; trial++) {
        /* A text of lines of random lengths, with tabs and empty lines, and maybe a newline at the end */
        std::string text;
        int lines = 1 + (int)(rng() % (rng() % 4 == 0 ? 20 : 400));
        int max_len = rng() % 3 == 0 ? 200 : 40;
        for (int l = 0; l < lines && text.size() < 60000; l++) {
            int len = (int)(rng() % (max_len + 1));
            for (int c = 0; c < len; c++) {
                int r = (int)(rng() % 20);
                text += r == 0 ? '\t' : (r == 1 ? ' ' : (char)('a' + r));
            }
            if (l < lines - 1 || rng() % 2) {
                text += '\n';
            }
        }
        if (text.size() > 60000) {
            text.resize(60000);
        }

        setText(e, text);
        int width = 200 + (int)(rng() % 800);
        int height = EDITOR_MIN_HEIGHT + (int)(rng() % 700);
        e->applyResolution(width, height);

        int cols = e->visibleColumns();
        int rows = e->visibleLines();
        std::vector<int> row_of = rowsOf(text, cols);
        int total_rows = row_of[text.size()] + 1;
        CHECK(total_rows == e->rowsFrom(0, cols), "trial %d: model's rows %d, Editor's %d", trial, total_rows, e->rowsFrom(0, cols));

        /* A view anywhere, possibly past the end, as F3 can leave it */
        e->scrollY = (int)(rng() % lineCount(text));
        e->m_scrollWrap = (int)(rng() % 3);
        e->normalizeScroll(cols);
        int before_y = e->scrollY;
        int before_wrap = e->m_scrollWrap;
        int before_first = row_of[e->viewStartIndex(cols)];

        int asked = (int)(rng() % (lineCount(text) + 20)) - 5;
        int line = clampInt(asked, 1, lineCount(text));
        int start = lineStartIndex(text, line);
        bool was_visible = row_of[start] - before_first >= 0 && row_of[start] - before_first < rows;

        e->m_selAnchor = rng() % 2 ? 0 : -1;
        e->goToLine(asked);

        CHECK(e->m_bufferEdit == start, "trial %d: line %d: cursor %d, want %d", trial, line, e->m_bufferEdit, start);
        CHECK(e->lineNumberAtIndex(e->m_bufferEdit) == line, "trial %d: on line %d, want %d", trial, e->lineNumberAtIndex(e->m_bufferEdit), line);
        CHECK(e->m_selAnchor == -1 && e->m_preferredColumn == -1, "trial %d: selection or column kept", trial);

        int first = row_of[e->viewStartIndex(cols)];
        int cursor_row = row_of[start] - first;
        CHECK(cursor_row >= 0 && cursor_row < rows, "trial %d: cursor row %d of %d", trial, cursor_row, rows);

        if (was_visible) {
            stayed++;
            CHECK(e->scrollY == before_y && e->m_scrollWrap == before_wrap, "trial %d: line %d in view, but the view moved", trial, line);
        } else {
            moved++;
            int want = row_of[start] - (rows / 2);
            int last_first = total_rows - rows > 0 ? total_rows - rows : 0;
            if (want > last_first) {
                want = last_first;
            }
            if (want < 0) {
                want = 0;
            }
            centered += want == row_of[start] - (rows / 2);
            at_end += want == last_first && want != row_of[start] - (rows / 2);
            at_top += want == 0 && want != row_of[start] - (rows / 2);
            CHECK(first == want, "trial %d: line %d (row %d of %d, %d rows shown): view starts at row %d, want %d", trial, line, row_of[start], total_rows, rows, first, want);
        }
    }

    printf("goToLine: %d trials, %d in view and left, %d moved: %d centered, %d at the end, %d at the top\n", trials, stayed, moved, centered, at_end, at_top);
    delete e;
}

/* Fixed cases of where the view lands, in the default window's 27 rows */
static void testGoToLineFixed()
{
    Editor* e = new Editor();
    std::string text = numberedLines(300, true);
    setText(e, text);
    int cols = e->visibleColumns();
    int rows = e->visibleLines();
    CHECK(rows == 27, "rows %d", rows);

    /* In view from the top: nothing scrolls, even on the bottom row */
    e->goToLine(27);
    CHECK(e->scrollY == 0, "line 27 scrolled to %d", e->scrollY);
    e->goToLine(10);
    CHECK(e->scrollY == 0, "line 10 scrolled to %d", e->scrollY);

    /* Below the view: in the middle, 13 rows down */
    e->goToLine(150);
    CHECK(e->scrollY == 149 - 13, "line 150: view at line %d", e->scrollY + 1);

    /* The last line: at the bottom, with the empty line after the last newline below it */
    e->goToLine(301);
    CHECK(e->scrollY == 301 - 27, "line 301: view at line %d", e->scrollY + 1);
    e->goToLine(290);
    CHECK(e->scrollY == 301 - 27, "line 290, already in view: view at line %d", e->scrollY + 1);

    /* Back near the top: from the top */
    e->goToLine(3);
    CHECK(e->scrollY == 0, "line 3: view at line %d", e->scrollY + 1);

    /* A short text that fits: never scrolls */
    setText(e, numberedLines(10, false));
    for (int l = -1; l <= 12; l++) {
        e->goToLine(l);
        CHECK(e->scrollY == 0 && e->m_scrollWrap == 0, "short text, line %d: scrolled", l);
    }

    /* An empty text */
    setText(e, "");
    e->goToLine(5);
    CHECK(e->m_bufferEdit == 0 && e->scrollY == 0, "empty text");

    /* Lines that wrap: the view counts rows, not lines */
    std::string wrapped;
    for (int i = 0; i < 100; i++) {
        wrapped += std::string(cols * 2 + 3, 'x') + "\n";
    }
    setText(e, wrapped);
    e->goToLine(50);
    int first = e->viewStartIndex(cols);
    int row = 0;
    int col = 0;
    e->cursorFromViewStart(first, cols, &row, &col);
    CHECK(row == rows / 2, "wrapped: cursor on row %d", row);
    (void)col;
    delete e;
}

/* The prompt's label and cursor at every width: the cursor only where it fits, and the short label when the long one leaves no room */
static void testPromptLayout()
{
    Editor* e = new Editor();
    /* As many lines as the Editor holds: 65,535 newlines, its most characters */
    std::string many(65535, '\n');
    struct { std::string text; int lines; } texts[] = {{"", 1}, {numberedLines(300, false), 300}, {many, 65536}};

    for (auto& t : texts) {
        setText(e, t.text);
        for (int width = 60; width <= 1000; width += 7) {
            e->applyResolution(width, EDITOR_DEFAULT_HEIGHT);
            int max_chars = e->textClipWidth() / 8;
            if (max_chars < 1) {
                max_chars = 1;
            }
            for (const char* typed : {"", "7", "12345"}) {
                e->m_prompt = EDITOR_PROMPT_LINE;
                strcpy(e->m_promptText, typed);
                e->m_promptLen = (int)strlen(typed);
                g_calls.clear();
                e->drawStatus();

                const DrawCall* status = lastStatusText(e);
                std::string full = "Go to line (1-" + std::to_string(t.lines) + "): " + typed;
                std::string shortl = std::string("Line: ") + typed;
                std::string want = (int)full.size() + 1 <= max_chars ? full : shortl;
                if ((int)want.size() > max_chars) {
                    want = want.substr(0, max_chars);
                }
                CHECK(status != nullptr && status->text == want, "width %d, '%s': '%s', want '%s'", width, typed, status ? status->text.c_str() : "(none)", want.c_str());

                const DrawCall* cursor = promptCursorAfterStatus(e);
                std::string drawn = (int)full.size() + 1 <= max_chars ? full : shortl;
                bool fits = (int)drawn.size() < max_chars;
                CHECK((cursor != nullptr) == fits, "width %d, '%s': cursor %s", width, typed, cursor ? "drawn" : "missing");
                if (cursor != nullptr) {
                    CHECK(cursor->x == e->textStartX() + (int)drawn.size() * 8, "width %d: cursor at %d", width, cursor->x);
                    CHECK(cursor->x + 8 <= e->textStartX() + max_chars * 8, "width %d: cursor past the field", width);
                }
            }
        }
    }
    e->m_prompt = EDITOR_PROMPT_NONE;
    delete e;
}


/* Through EditorLoop: a press closes the prompt and then does what it does; a resize keeps it open */
static void testLoop()
{
    Editor* e = new Editor();
    std::string text = numberedLines(300, true);
    setText(e, text);

    /* Ctrl+G, 9, then a click on row 2, column 3 of the text, then typing: the 7 goes into the text where the click put the cursor */
    int x = e->textStartX() + (3 * 8) + 2;
    int y = TEXT_TOP + (2 * TEXT_ROW_HEIGHT) + 2;
    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, CTRLG));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '9'));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE_DOWN, x, y, 1 << GFX_EVENT_CLICKS_SHIFT));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE_UP, x, y));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE, x, y));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '7'));
    CHECK(runLoop(e), "the loop didn't exit on the close");

    int at = lineStartIndex(text, 3) + 3;
    std::string want = text;
    want.insert(at, "7");
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "prompt still open after a click");
    CHECK(e->m_bufferHead == (int)want.size() && memcmp(e->m_textBuffer, want.data(), want.size()) == 0, "the 7 didn't go in at the click");
    CHECK(e->m_bufferEdit == at + 1, "cursor %d, want %d", e->m_bufferEdit, at + 1);

    /* Ctrl+G, 4, 2, a resize, then Enter: the prompt was redrawn after the resize, and Enter goes to line 42 */
    setText(e, text);
    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, CTRLG));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '4'));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '2'));
    g_events.push_back(testEvent(GFX_EVENT_RESOLUTION, 700, 500));
    CHECK(runLoop(e), "the loop didn't exit on the close");
    const DrawCall* status = lastStatusText(e);
    CHECK(e->m_prompt == EDITOR_PROMPT_LINE && status != nullptr && status->text == "Go to line (1-301): 42", "after a resize: '%s'", status ? status->text.c_str() : "(none)");
    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '\n'));
    CHECK(runLoop(e), "the loop didn't exit on the close");
    CHECK(e->m_bufferEdit == lineStartIndex(text, 42), "after a resize, Enter went to %d", e->m_bufferEdit);

    /* A press on a toolbar button closes the prompt too */
    setText(e, text);
    int bx = 0;
    e->buttonPosition(EDITOR_BUTTON_HELP, &bx);
    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, CTRLG));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE_DOWN, bx + 4, EDITOR_BUTTON_Y + 4, 1 << GFX_EVENT_CLICKS_SHIFT));
    CHECK(runLoop(e), "the loop didn't exit on the close");
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "prompt still open after a press on a button");
    status = lastStatusText(e);
    CHECK(status != nullptr && status->text.find("Ln:1 Col:1") != std::string::npos, "status after the press: '%s'", status ? status->text.c_str() : "(none)");
    delete e;
}

/* What drawPrompt drew: the status text, the highlight behind filled-in text and the text over it, the cursor, and the count or "Not found" */
struct PromptDraw {
    const DrawCall* status;
    const DrawCall* highlight;
    const DrawCall* highlightText;
    const DrawCall* cursor;
    const DrawCall* suffix;
};

static PromptDraw promptDraw(Editor* e)
{
    PromptDraw d = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int y = e->c_height - STATUS_HEIGHT + EDITOR_STATUS_MARGIN + 2;
    int first = -1;
    for (int i = (int)g_calls.size() - 1; i >= 0; i--) {
        if (g_calls[i].kind == 'f' && g_calls[i].y == y && g_calls[i].x == e->textStartX()) {
            first = i;
            break;
        }
    }
    if (first < 0) {
        return d;
    }
    d.status = &g_calls[first];
    for (int i = first + 1; i < (int)g_calls.size(); i++) {
        const DrawCall& c = g_calls[i];
        if (c.kind == 'r' && c.y == y - 1 && c.h == 10) {
            d.highlight = &c;
        } else if (c.kind == 'f' && c.y == y && d.highlight != nullptr && d.highlightText == nullptr && d.cursor == nullptr) {
            d.highlightText = &c;
        } else if (c.kind == 'r' && c.y == y && c.w == 8 && c.h == 8) {
            d.cursor = &c;
        } else if (c.kind == 'f' && c.y == y) {
            d.suffix = &c;
        }
    }
    return d;
}

static std::vector<int> matchesOf(const std::string& text, const std::string& needle)
{
    std::vector<int> found;
    for (size_t i = 0; needle.size() > 0 && i + needle.size() <= text.size(); i++) {
        if (text.compare(i, needle.size(), needle) == 0) {
            found.push_back((int)i);
        }
    }
    return found;
}

static void typeAt(Editor* e, const char* s)
{
    for (const char* p = s; *p; p++) {
        key(e, (unsigned char)*p);
    }
}

#define SAME_TEXT(e, text) ((e)->m_bufferHead == (int)(text).size() && memcmp((e)->m_textBuffer, (text).data(), (text).size()) == 0)
#define SELECTED(e, s, len) ((e)->m_selAnchor == (s) && (e)->m_bufferEdit == (s) + (len))

static void testFindKeys()
{
    Editor* e = new Editor();
    std::string text = numberedLines(300, true);
    setText(e, text);
    e->reDraw(0, e->m_bufferHead);
    int x = e->textStartX();
    std::vector<int> line1 = matchesOf(text, "line 1");
    CHECK(line1.size() == 111, "%d matches of line 1", (int)line1.size());

    /* Ctrl+F with nothing selected and nothing searched for yet: an empty prompt */
    g_calls.clear();
    key(e, CTRLF);
    PromptDraw d = promptDraw(e);
    CHECK(e->m_prompt == EDITOR_PROMPT_FIND, "prompt %d", e->m_prompt);
    CHECK(d.status != nullptr && d.status->text == "Find: ", "status '%s'", d.status ? d.status->text.c_str() : "(none)");
    CHECK(d.status != nullptr && d.status->color == COLOR_BLACK, "status color");
    CHECK(d.cursor != nullptr && d.cursor->x == x + 6 * 8, "cursor %d", d.cursor ? d.cursor->x : -1);
    CHECK(d.highlight == nullptr && d.suffix == nullptr, "highlight or suffix drawn");

    /* Typing, and keys that do nothing, to the prompt or the text */
    typeAt(e, "line 1");
    unsigned char ignored[] = {KEY_LEFT, KEY_UP, KEY_HOME, KEY_PAGE_DOWN, KEY_DELETE, KEY_F3, CTRLA, CTRLB, CTRLG, CTRLV, CTRLX, CTRLZ, CTRLS, '\t', 0, 127, 200};
    for (unsigned char c : ignored) {
        key(e, c);
    }
    CHECK(std::string(e->m_promptText) == "line 1", "typed '%s'", e->m_promptText);
    CHECK(SAME_TEXT(e, text) && e->m_bufferEdit == 0 && e->m_selAnchor == -1 && !e->m_treeCollapsed, "a key reached the Editor");

    /* Enter: the first match at the cursor or after it, and the prompt stays open, with the count */
    g_calls.clear();
    key(e, '\n');
    d = promptDraw(e);
    CHECK(e->m_prompt == EDITOR_PROMPT_FIND, "closed after Enter");
    CHECK(SELECTED(e, 0, 6), "selected %d to %d", e->m_selAnchor, e->m_bufferEdit);
    CHECK(d.status != nullptr && d.status->text == "Find: line 1", "status '%s'", d.status ? d.status->text.c_str() : "(none)");
    CHECK(d.cursor != nullptr && d.cursor->x == x + 12 * 8, "cursor %d", d.cursor ? d.cursor->x : -1);
    CHECK(d.suffix != nullptr && d.suffix->text == "1 of 111" && d.suffix->x == x + 14 * 8 && d.suffix->color == COLOR_VGA_MEDIUM_DARK_GRAY,
          "suffix '%s' at %d", d.suffix ? d.suffix->text.c_str() : "(none)", d.suffix ? d.suffix->x : -1);

    struct { unsigned char c; int mods; int at; const char* count; } steps[] = {
        {'\n', 0, lineStartIndex(text, 10), "2 of 111"},
        {'\r', 0, lineStartIndex(text, 11), "3 of 111"},
        {'\n', KEY_MOD_SHIFT, lineStartIndex(text, 10), "2 of 111"},
        {'\n', KEY_MOD_SHIFT, 0, "1 of 111"},
        /* Round the start to the last, and the end to the first */
        {'\n', KEY_MOD_SHIFT, lineStartIndex(text, 199), "111 of 111"},
        {'\n', 0, 0, "1 of 111"},
    };
    for (auto& s : steps) {
        g_calls.clear();
        key(e, s.c, s.mods);
        d = promptDraw(e);
        CHECK(SELECTED(e, s.at, 6), "want %d: selected %d to %d", s.at, e->m_selAnchor, e->m_bufferEdit);
        CHECK(d.suffix != nullptr && d.suffix->text == s.count, "want %s: '%s'", s.count, d.suffix ? d.suffix->text.c_str() : "(none)");
    }

    /* Editing what's typed takes the count away; not found leaves the selection, and says so in red */
    g_calls.clear();
    key(e, '\b');
    d = promptDraw(e);
    CHECK(std::string(e->m_promptText) == "line " && d.suffix == nullptr, "after Backspace: '%s', suffix %s", e->m_promptText, d.suffix ? "drawn" : "gone");
    for (int i = 0; i < 5; i++) {
        key(e, '\b');
    }
    typeAt(e, "zzz");
    g_calls.clear();
    key(e, '\n');
    d = promptDraw(e);
    CHECK(SELECTED(e, 0, 6), "not found moved the selection");
    CHECK(d.suffix != nullptr && d.suffix->text == "Not found" && d.suffix->color == COLOR_VGA_RED, "'%s'", d.suffix ? d.suffix->text.c_str() : "(none)");
    CHECK(e->m_keepStatus == 0, "Not found kept the status line");
    g_calls.clear();
    key(e, 'q');
    d = promptDraw(e);
    CHECK(std::string(e->m_promptText) == "zzzq" && d.suffix == nullptr, "after typing: '%s', suffix %s", e->m_promptText, d.suffix ? "drawn" : "gone");
    key(e, '\b');

    /* Esc: closed, with the match still selected */
    key(e, 27);
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE && SELECTED(e, 0, 6), "Esc");

    /* Ctrl+F with "line 1" selected: filled in with it, highlighted, and the first key replaces it */
    g_calls.clear();
    key(e, CTRLF);
    d = promptDraw(e);
    CHECK(std::string(e->m_promptText) == "line 1" && e->m_promptFilled, "filled '%s'", e->m_promptText);
    CHECK(d.highlight != nullptr && d.highlight->x == x + 6 * 8 && d.highlight->w == 6 * 8 && d.highlight->color == COLOR_SELECTED, "highlight");
    CHECK(d.highlightText != nullptr && d.highlightText->text == "line 1" && d.highlightText->x == x + 6 * 8, "highlighted text");
    CHECK(d.cursor != nullptr && d.cursor->x == x + 12 * 8, "cursor after filled text");
    key(e, 'q');
    CHECK(std::string(e->m_promptText) == "q" && !e->m_promptFilled, "typed over: '%s'", e->m_promptText);
    key(e, 27);

    /* Enter on what's filled in searches for it, from the selection: line 10. It stays filled in, so typing still replaces it */
    key(e, CTRLF);
    g_calls.clear();
    key(e, '\n');
    d = promptDraw(e);
    CHECK(SELECTED(e, lineStartIndex(text, 10), 6), "Enter on filled text: %d", e->m_selAnchor);
    CHECK(e->m_promptFilled && d.highlight != nullptr && d.suffix != nullptr && d.suffix->text == "2 of 111", "after Enter on filled text");
    /* Backspace takes all of it */
    key(e, '\b');
    CHECK(e->m_promptLen == 0 && !e->m_promptFilled, "Backspace on filled text left '%s'", e->m_promptText);
    key(e, 27);

    /* Without a selection, the last search */
    e->clearSelection();
    key(e, CTRLF);
    CHECK(std::string(e->m_promptText) == "line 1" && e->m_promptFilled, "last search '%s'", e->m_promptText);
    key(e, 27);

    /* A selection over lines: the last search */
    e->m_selAnchor = lineStartIndex(text, 3);
    e->m_bufferEdit = lineStartIndex(text, 5);
    key(e, CTRLF);
    CHECK(std::string(e->m_promptText) == "line 1", "selection over lines: '%s'", e->m_promptText);
    key(e, 27);

    /* The prompt's own key closes it, and the other prompt's does nothing */
    key(e, CTRLF);
    key(e, CTRLG);
    CHECK(e->m_prompt == EDITOR_PROMPT_FIND, "Ctrl+G at Find");
    key(e, CTRLF);
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "Ctrl+F didn't close Find");
    key(e, CTRLG);
    key(e, CTRLF);
    CHECK(e->m_prompt == EDITOR_PROMPT_LINE, "Ctrl+F at Go to line");
    key(e, 27);

    /* Enter with nothing typed only closes it */
    e->clearSelection();
    e->m_bufferEdit = 40;
    e->m_findText[0] = 0;
    key(e, CTRLF);
    CHECK(e->m_promptLen == 0, "filled with '%s'", e->m_promptText);
    key(e, '\n');
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE && e->m_bufferEdit == 40 && e->m_selAnchor == -1, "empty Enter");
    CHECK(SAME_TEXT(e, text), "text changed");

    /* Selections with a tab, or longer than the prompt holds, aren't filled in; 64 printable characters are */
    std::string long_line = std::string(64, 'k') + "m\n" + "a\tb\n";
    setText(e, long_line);
    strcpy(e->m_findText, "prev");
    e->m_selAnchor = 0;
    e->m_bufferEdit = 64;
    key(e, CTRLF);
    CHECK(std::string(e->m_promptText) == std::string(64, 'k'), "64 characters: %d", e->m_promptLen);
    key(e, 27);
    e->m_selAnchor = 0;
    e->m_bufferEdit = 65;
    key(e, CTRLF);
    CHECK(std::string(e->m_promptText) == "prev", "65 characters: '%s'", e->m_promptText);
    key(e, 27);
    e->m_selAnchor = 66;
    e->m_bufferEdit = 69;
    key(e, CTRLF);
    CHECK(std::string(e->m_promptText) == "prev", "a tab: '%s'", e->m_promptText);
    key(e, 27);

    /* At most 64 characters, and only 32 to 126 */
    e->clearSelection();
    e->m_findText[0] = 0;
    key(e, CTRLF);
    for (int i = 0; i < 70; i++) {
        key(e, (unsigned char)('!' + (i % 90)));
    }
    CHECK(e->m_promptLen == EDITOR_PROMPT_MAX, "%d characters", e->m_promptLen);
    key(e, 27);

    /* Overlapping matches each count: "aa" in "aaaa" is 3 */
    setText(e, "aaaa");
    key(e, CTRLF);
    typeAt(e, "aa");
    for (int i = 0; i < 4; i++) {
        key(e, '\n');
        CHECK(SELECTED(e, i % 3, 2) && e->m_findIndex == (i % 3) + 1 && e->m_findCount == 3, "step %d: %d of %d at %d", i, e->m_findIndex, e->m_findCount, e->m_selAnchor);
    }
    key(e, 27);

    /* A match starting on the view's bottom row and wrapping onto the next: scrolled so its end, and the cursor, are in view */
    {
        int cols = e->visibleColumns();
        int rows = e->visibleLines();
        std::string wrap;
        for (int i = 0; i < rows - 1; i++) {
            wrap += "x\n";
        }
        wrap += std::string(cols - 2, 'a') + "NEEDLE\n";
        setText(e, wrap);
        e->reDraw(0, e->m_bufferHead);
        key(e, CTRLF);
        typeAt(e, "NEEDLE");
        key(e, '\n');
        int at = (int)wrap.find("NEEDLE");
        CHECK(SELECTED(e, at, 6), "wrapping match: selected %d", e->m_selAnchor);
        int row = 0;
        int col = 0;
        e->cursorFromViewStart(e->viewStartIndex(cols), cols, &row, &col);
        CHECK(row >= 0 && row < rows, "wrapping match: cursor on row %d of %d", row, rows);
        key(e, 27);
    }

    /* Case counts */
    setText(e, "Abc abc ABC");
    key(e, CTRLF);
    typeAt(e, "abc");
    key(e, '\n');
    CHECK(SELECTED(e, 4, 3) && e->m_findCount == 1, "case: %d of %d at %d", e->m_findIndex, e->m_findCount, e->m_selAnchor);
    key(e, 27);

    /* A read-only file can be searched */
    setText(e, text);
    e->m_readOnly = true;
    key(e, CTRLF);
    typeAt(e, "line 250");
    key(e, '\n');
    CHECK(SELECTED(e, lineStartIndex(text, 250), 8), "read-only: %d", e->m_selAnchor);
    key(e, 27);
    e->m_readOnly = false;

    /* Typing over a match found is an edit like any other, which undo takes back */
    key(e, 'X');
    std::string want = text;
    want.replace(lineStartIndex(text, 250), 8, "X");
    CHECK(SAME_TEXT(e, want), "typing over the match");
    e->undo();
    CHECK(SAME_TEXT(e, text) && SELECTED(e, lineStartIndex(text, 250), 8), "undo after typing over the match");

    delete e;
}

/**
 * findNext on random texts, searches, selections, windows and views, against a model: the next match at or after the
 * cursor, or after the selection's start, the one before, going round the end, and the view as revealRange should leave it
 */
static void testFindRandom(std::mt19937& rng, int trials)
{
    Editor* e = new Editor();
    int found_count = 0;
    int none = 0;
    int stayed = 0;
    int moved = 0;
    int wrapped = 0;
    int spanning = 0;
    int half_in = 0;
    const char* alphabets[] = {"ab\n", "ab \n\t", "abcdefgh \n", "x\n"};

    for (int trial = 0; trial < trials; trial++) {
        const char* alphabet = alphabets[rng() % 4];
        int alen = (int)strlen(alphabet);
        int n = (int)(rng() % (rng() % 4 == 0 ? 20000 : 2000));
        std::string text;
        for (int i = 0; i < n; i++) {
            text += alphabet[rng() % alen];
        }
        std::string needle;
        int nlen = 1 + (int)(rng() % (rng() % 8 == 0 ? 64 : 4));
        for (int i = 0; i < nlen; i++) {
            char c = alphabet[rng() % alen];
            needle += (c == '\n' || c == '\t') ? 'a' : c;
        }

        /* Half the time, a piece of the text itself, up to its first newline or tab, as the prompt can't hold those */
        if (n > 0 && rng() % 2) {
            int at = (int)(rng() % n);
            int len = 1 + (int)(rng() % EDITOR_PROMPT_MAX);
            std::string piece = text.substr(at, len);
            size_t stop = piece.find_first_of("\n\t");
            if (stop != std::string::npos) {
                piece = piece.substr(0, stop);
            }
            if (!piece.empty()) {
                needle = piece;
            }
        }

        setText(e, text);
        e->applyResolution(200 + (int)(rng() % 800), EDITOR_MIN_HEIGHT + (int)(rng() % 700));
        int cols = e->visibleColumns();
        int rows = e->visibleLines();
        std::vector<int> row_of = rowsOf(text, cols);
        int total_rows = row_of[text.size()] + 1;

        e->scrollY = (int)(rng() % lineCount(text));
        e->m_scrollWrap = (int)(rng() % 3);
        e->normalizeScroll(cols);
        e->m_bufferEdit = (int)(rng() % (text.size() + 1));
        e->m_selAnchor = rng() % 2 ? (int)(rng() % (text.size() + 1)) : -1;
        bool backward = rng() % 2;

        int edit = e->m_bufferEdit;
        int anchor = e->m_selAnchor;
        int before_y = e->scrollY;
        int before_wrap = e->m_scrollWrap;
        int before_first = row_of[e->viewStartIndex(cols)];
        bool selected = anchor >= 0 && anchor != edit;
        int sel_start = selected ? std::min(anchor, edit) : edit;

        e->m_prompt = EDITOR_PROMPT_FIND;
        strcpy(e->m_promptText, needle.c_str());
        e->m_promptLen = (int)needle.size();
        e->findNext(backward);

        std::vector<int> p = matchesOf(text, needle);
        CHECK(e->m_findCount == (int)p.size(), "trial %d: count %d, want %d", trial, e->m_findCount, (int)p.size());
        CHECK(std::string(e->m_findText) == needle, "trial %d: kept '%s'", trial, e->m_findText);
        CHECK(SAME_TEXT(e, text), "trial %d: text changed", trial);

        if (p.empty()) {
            none++;
            CHECK(e->m_bufferEdit == edit && e->m_selAnchor == anchor && e->scrollY == before_y && e->m_scrollWrap == before_wrap, "trial %d: none found, but something moved", trial);
            continue;
        }

        int want = -1;
        if (backward) {
            for (int i = (int)p.size() - 1; i >= 0 && want < 0; i--) {
                want = p[i] < sel_start ? p[i] : -1;
            }
            if (want < 0) {
                want = p.back();
                wrapped++;
            }
        } else {
            int from = selected ? sel_start + 1 : edit;
            for (size_t i = 0; i < p.size() && want < 0; i++) {
                want = p[i] >= from ? p[i] : -1;
            }
            if (want < 0) {
                want = p.front();
                wrapped++;
            }
        }
        found_count++;
        int index = (int)(std::find(p.begin(), p.end(), want) - p.begin()) + 1;
        CHECK(SELECTED(e, want, (int)needle.size()), "trial %d: selected %d to %d, want %d", trial, e->m_selAnchor, e->m_bufferEdit, want);
        CHECK(e->m_findIndex == index, "trial %d: %d of %d, want %d", trial, e->m_findIndex, e->m_findCount, index);

        int rs = row_of[want];
        int re = row_of[want + needle.size()];
        spanning += re > rs;
        half_in += rs - before_first >= 0 && rs - before_first < rows && !(re - before_first < rows);
        int first = row_of[e->viewStartIndex(cols)];
        CHECK(re - first >= 0 && re - first < rows, "trial %d: cursor row %d not in view", trial, re - first);
        bool was_visible = rs - before_first >= 0 && rs - before_first < rows && re - before_first >= 0 && re - before_first < rows;
        if (was_visible) {
            stayed++;
            CHECK(e->scrollY == before_y && e->m_scrollWrap == before_wrap, "trial %d: match in view, but the view moved", trial);
        } else {
            moved++;
            int f = rs - (rows / 2);
            int last_first = total_rows - rows > 0 ? total_rows - rows : 0;
            f = std::min(f, last_first);
            f = std::max(f, 0);
            if (re - f >= rows) {
                f = re - rows + 1;
            }
            CHECK(first == f, "trial %d: match rows %d to %d of %d, %d shown: view at %d, want %d", trial, rs, re, total_rows, rows, first, f);
        }
    }
    printf("findNext: %d trials, %d found (%d round the end), %d with none; %d in view and left, %d moved; %d over rows, %d starting in view and ending below it\n", trials, found_count, wrapped, none, stayed, moved, spanning, half_in);
    delete e;
}

/* The Find prompt at every width: everything inside the field, the end of what's typed shown, and the count kept when there's room */
static void testFindLayout()
{
    Editor* e = new Editor();
    setText(e, numberedLines(50, true));
    std::string long_text;
    for (int i = 0; i < EDITOR_PROMPT_MAX; i++) {
        long_text += (char)('a' + (i % 26));
    }
    const char* typed_cases[] = {"", "x", "printf", "abcdefghijklmnopqrstuvwxyz0123456789"};
    struct { int count; int index; const char* suffix; } suffixes[] = {{-1, 0, ""}, {0, 0, "Not found"}, {12, 3, "3 of 12"}, {1000, 999, "999 of 1000"}};
    int checked = 0;

    for (int width = 60; width <= 1000; width += 3) {
        e->applyResolution(width, EDITOR_DEFAULT_HEIGHT);
        int x = e->textStartX();
        int max_chars = e->textClipWidth() / 8;
        for (int t = 0; t < 5; t++) {
            std::string typed = t < 4 ? typed_cases[t] : long_text;
            for (auto& s : suffixes) {
                for (int filled = 0; filled < 2; filled++) {
                    e->m_prompt = EDITOR_PROMPT_FIND;
                    strcpy(e->m_promptText, typed.c_str());
                    e->m_promptLen = (int)typed.size();
                    e->m_promptFilled = filled && !typed.empty();
                    e->m_findCount = s.count;
                    e->m_findIndex = s.index;
                    g_calls.clear();
                    e->drawStatus();
                    PromptDraw d = promptDraw(e);
                    checked++;

                    CHECK(d.status != nullptr && d.status->text.compare(0, std::min(6, max_chars), std::string("Find: ").substr(0, std::min(6, max_chars))) == 0,
                          "width %d: '%s'", width, d.status ? d.status->text.c_str() : "(none)");
                    if (d.status == nullptr) {
                        continue;
                    }
                    int drawn = (int)d.status->text.size();
                    CHECK(drawn <= max_chars, "width %d: status %d characters in %d", width, drawn, max_chars);

                    /* What's typed: whole, or its end after "..." when there's room for more than the dots */
                    std::string shown = d.status->text.substr(std::min(6, drawn));
                    bool cut = shown.compare(0, 3, "...") == 0 && typed.compare(0, 3, "...") != 0;
                    if (cut) {
                        std::string tail = shown.substr(3);
                        CHECK(typed.size() >= tail.size() && typed.compare(typed.size() - tail.size(), tail.size(), tail) == 0,
                              "width %d: '%s' isn't the end of '%s'", width, shown.c_str(), typed.c_str());
                        CHECK(d.cursor != nullptr, "width %d: cut short, but no cursor", width);
                    }

                    /* The cursor right after it, and the suffix two cells on, all inside the field */
                    if (d.cursor != nullptr) {
                        CHECK(d.cursor->x == x + drawn * 8, "width %d: cursor at %d, text ends at %d", width, d.cursor->x, x + drawn * 8);
                        CHECK(d.cursor->x + 8 <= x + max_chars * 8, "width %d: cursor past the field", width);
                    }
                    if (d.suffix != nullptr) {
                        CHECK(d.suffix->text == s.suffix, "width %d: suffix '%s', want '%s'", width, d.suffix->text.c_str(), s.suffix);
                        CHECK(d.suffix->x == x + (drawn + 2) * 8, "width %d: suffix at %d", width, d.suffix->x);
                        CHECK(d.suffix->x + (int)d.suffix->text.size() * 8 <= x + max_chars * 8, "width %d: suffix past the field", width);
                        CHECK(d.suffix->color == (s.count == 0 ? COLOR_VGA_RED : COLOR_VGA_MEDIUM_DARK_GRAY), "suffix color");
                    }

                    /* With room for all of it, everything is drawn, uncut */
                    int all = 6 + (int)typed.size() + 1 + (s.suffix[0] ? 1 + (int)strlen(s.suffix) : 0);
                    if (all <= max_chars) {
                        CHECK(!cut && shown == typed && d.cursor != nullptr && (s.suffix[0] == 0) == (d.suffix == nullptr),
                              "width %d, '%s', '%s': room for all, but '%s'", width, typed.c_str(), s.suffix, d.status->text.c_str());
                    }
                    /* The suffix is kept whenever 4 characters of what's typed fit beside it, with the cursor */
                    if (s.suffix[0] && 6 + std::min((int)typed.size(), 4) + 1 + 1 + (int)strlen(s.suffix) <= max_chars) {
                        CHECK(d.suffix != nullptr, "width %d, '%s', '%s': suffix dropped", width, typed.c_str(), s.suffix);
                    }

                    /* The highlight behind filled-in text, exactly behind what's shown of it */
                    if (e->m_promptFilled && drawn <= max_chars && drawn > 6) {
                        CHECK(d.highlight != nullptr && d.highlight->x == x + 6 * 8 && d.highlight->w == (drawn - 6) * 8, "width %d: highlight", width);
                        CHECK(d.highlightText != nullptr && d.highlightText->text == shown, "width %d: highlighted text", width);
                    } else if (!e->m_promptFilled) {
                        CHECK(d.highlight == nullptr, "width %d: highlight without filled text", width);
                    }
                }
            }
        }
    }
    e->closePrompt();
    printf("Find layout: %d prompts drawn\n", checked);
    delete e;
}

/* Through EditorLoop: Find, Shift+Enter, a resize, and a click that closes it */
static void testFindLoop()
{
    Editor* e = new Editor();
    std::string text = numberedLines(300, true);
    setText(e, text);

    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, CTRLF));
    for (const char* p = "line 2"; *p; p++) {
        g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, *p));
    }
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '\n'));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '\n'));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '\n', 0, KEY_MOD_SHIFT));
    g_events.push_back(testEvent(GFX_EVENT_KEYBOARD, '\n', 0, KEY_MOD_SHIFT));
    g_events.push_back(testEvent(GFX_EVENT_RESOLUTION, 700, 500));
    CHECK(runLoop(e), "the loop didn't exit on the close");
    std::vector<int> p = matchesOf(text, "line 2");
    PromptDraw d = promptDraw(e);
    CHECK(e->m_prompt == EDITOR_PROMPT_FIND, "Find closed");
    CHECK(SELECTED(e, p.back(), 6), "selected %d, want %d", e->m_selAnchor, p.back());
    std::string count = std::to_string(p.size()) + " of " + std::to_string(p.size());
    CHECK(d.suffix != nullptr && d.suffix->text == count, "after a resize: '%s', want '%s'", d.suffix ? d.suffix->text.c_str() : "(none)", count.c_str());

    /* A click in the text closes it, and puts the cursor there */
    e->applyResolution(EDITOR_DEFAULT_WINDOW_WIDTH, EDITOR_DEFAULT_HEIGHT);
    e->reDraw(0, e->m_bufferHead);
    int view = e->viewStartIndex(e->visibleColumns());
    int x = e->textStartX() + (2 * 8) + 2;
    int y = TEXT_TOP + 2;
    g_events.clear();
    g_events.push_back(testEvent(GFX_EVENT_MOUSE_DOWN, x, y, 1 << GFX_EVENT_CLICKS_SHIFT));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE_UP, x, y));
    g_events.push_back(testEvent(GFX_EVENT_MOUSE, x, y));
    CHECK(runLoop(e), "the loop didn't exit on the close");
    CHECK(e->m_prompt == EDITOR_PROMPT_NONE, "Find open after a click");
    CHECK(e->m_bufferEdit == view + 2 && !e->hasSelection(), "click: cursor %d, want %d", e->m_bufferEdit, view + 2);
    delete e;
}

/* The text as it is, for comparing with a model */
static std::string textOf(Editor* e)
{
    return std::string((const char*)e->m_textBuffer, e->m_bufferHead);
}

static void pressTimes(Editor* e, unsigned char c, int times, int mods = 0)
{
    for (int i = 0; i < times; i++) {
        key(e, c, mods);
    }
}

/* A key whose drawing isn't looked at: forgetting it keeps g_calls from growing through thousands of keys */
static void press(Editor* e, unsigned char c, int mods = 0)
{
    key(e, c, mods);
    g_calls.clear();
}

/**
 * Undo and redo with the text at the Editor's limit (#213 step 3). Undo used to put the text coming in before taking
 * the text going out, so for a moment the text held both; it now takes out first, and must still get both ways right
 */
static void testUndoAtLimit()
{
    Editor* e = new Editor();
    int most = e->m_bufferSize - 1;

    /* A nearly full text, replaced by a full one with Ctrl+A and Ctrl+V, then undone and redone */
    std::string before = std::string(most - 10, 'a');
    std::string full = std::string(most, 'b');
    setText(e, before);
    clipboard_set(full.data(), (int)full.size());
    press(e, CTRLA);
    press(e, CTRLV);
    CHECK(textOf(e) == full, "paste over all: %d characters", e->m_bufferHead);
    press(e, CTRLZ);
    CHECK(textOf(e) == before, "undo to the nearly full text: %d characters", e->m_bufferHead);
    press(e, CTRLY);
    CHECK(textOf(e) == full, "redo to the full text: %d characters", e->m_bufferHead);
    press(e, CTRLZ);
    CHECK(textOf(e) == before, "undo again: %d characters", e->m_bufferHead);

    /* A paste that doesn't all fit goes in as far as it does, and undoes and redoes as that */
    setText(e, std::string(most - 5, 'c'));
    clipboard_set("0123456789", 10);
    e->m_bufferEdit = 100;
    press(e, CTRLV);
    std::string cut_short = std::string(100, 'c') + "01234" + std::string(most - 105, 'c');
    CHECK(textOf(e) == cut_short, "paste cut short: %d characters", e->m_bufferHead);
    press(e, CTRLZ);
    CHECK(textOf(e) == std::string(most - 5, 'c'), "undo the paste cut short");
    press(e, CTRLY);
    CHECK(textOf(e) == cut_short, "redo the paste cut short");

    /* A full text takes no more typing, and that changes nothing to undo */
    setText(e, full);
    e->m_bufferEdit = 50;
    press(e, 'x');
    CHECK(textOf(e) == full, "typing into a full text changed it");
    press(e, '\b');
    press(e, 'x');
    std::string typed = full;
    typed[49] = 'x';
    CHECK(textOf(e) == typed, "Backspace then typing in a full text");
    for (int i = 0; i < 3; i++) {
        press(e, CTRLZ);
    }
    CHECK(textOf(e) == full, "undo in a full text");
    delete e;
}

/**
 * Random edits near the limit, then undo until there's nothing left to undo, which must give back the text as it was
 * opened, and redo until there's nothing left to redo, which must give back the text as it was after the edits
 */
static void testUndoAllRandom(std::mt19937& rng, int trials)
{
    Editor* e = new Editor();
    int most = e->m_bufferSize - 1;
    int complete = 0;
    for (int trial = 0; trial < trials; trial++) {
        int size = (int)(rng() % 6 == 0 ? most - (int)(rng() % 64) : rng() % 3000);
        std::string text;
        for (int i = 0; i < size; i++) {
            text += "ab {}\n\t"[rng() % 7];
        }
        setText(e, text);

        /**
         * In a big text, edits stay near the cursor, as a user's mostly do, starting from the top or the middle. A far
         * one makes ensureCursorVisible scroll there a row at a time, each row a pass over the text (#213), which
         * under ASan takes seconds. Small texts get edits anywhere
         */
        bool big = size > 4000;
        if (big && rng() % 2) {
            press(e, CTRLG);
            for (char c : std::to_string(e->countLines() / 2)) {
                press(e, (unsigned char)c);
            }
            press(e, '\n');
        }
        auto somewhere = [&](int from) {
            int head = e->m_bufferHead;
            int at = big ? from + (int)(rng() % 601) - 300 : (int)(rng() % (head + 1));
            return clampInt(at, 0, head);
        };

        int edits = 1 + (int)(rng() % (big ? 12 : 25));
        for (int i = 0; i < edits; i++) {
            e->m_bufferEdit = somewhere(e->m_bufferEdit);
            e->m_selAnchor = rng() % 3 == 0 ? somewhere(e->m_bufferEdit) : -1;
            switch (rng() % 10) {
            case 0: press(e, '\b'); break;
            case 1: press(e, KEY_DELETE); break;
            case 2: press(e, '\n'); break;
            case 3: press(e, '\t'); break;
            case 4: press(e, CTRLX); break;
            case 5: press(e, CTRLC); press(e, CTRLV); break;
            case 6: press(e, CTRLV); break;
            case 7: press(e, '{'); break;
            default: press(e, "xyz}"[rng() % 4]); break;
            }
            CHECK(e->m_bufferHead <= most && e->m_textBuffer[e->m_bufferHead] == 0, "trial %d: %d characters, or no 0 after them", trial, e->m_bufferHead);
        }
        std::string after = textOf(e);
        if (e->m_undoFailed || e->m_undoDropped > 0) {
            continue;
        }
        complete++;
        /* Each press redraws, so only as many as there are steps, and a few over to be sure nothing's left */
        for (int guard = 3 * edits + 2; e->m_undoAt > 0 && guard > 0; guard--) {
            press(e, CTRLZ);
        }
        press(e, CTRLZ);
        press(e, CTRLZ);
        CHECK(textOf(e) == text, "trial %d: undoing %d edits on %d characters didn't give the text back", trial, edits, size);
        for (int guard = 3 * edits + 2; e->m_undoAt < e->m_undoUsed && guard > 0; guard--) {
            press(e, CTRLY);
        }
        press(e, CTRLY);
        press(e, CTRLY);
        CHECK(textOf(e) == after, "trial %d: redoing them didn't give the edited text back", trial);
    }
    printf("Undo all: %d of %d sessions undone and redone in full\n", complete, trials);
    delete e;
}

int main()
{
    std::mt19937 rng(137);
    testPromptKeys();
    testPromptRandom(rng);
    testGoToLineFixed();
    testGoToLineRandom(rng, 4000);
    testPromptLayout();
    testLoop();
    testFindKeys();
    testFindRandom(rng, 4000);
    testFindLayout();
    testFindLoop();
    testUndoAtLimit();
    testUndoAllRandom(rng, 120);
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}
