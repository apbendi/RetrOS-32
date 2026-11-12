/**
 * @file edit.cpp
 * @author Joe Bayer (joexbayer)
 * @brief A text editor app
 * @version 0.1
 * @date 2024-01-10
 */

#include "edit.hpp"

#include <utils/cppUtils.hpp>
#include <utils/MsgBox.hpp>
#include <utils/Thread.hpp>
#include <utils/Widgets.hpp>
#include <syscall_helper.h>
#include <errors.h>

extern "C" int invoke_syscall(int i, int arg1, int arg2, int arg3);

/* Helper functions */
static int isAlpha(unsigned char c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_');
}

static int minInt(int a, int b)
{
    return (a < b) ? a : b;
}

static int clampInt(int value, int lo, int hi)
{
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

/* The column after ch, drawn from column col of a row long enough: a tab ends at the next multiple of EDITOR_TAB_WIDTH, and anything else takes one column */
static int columnAfter(unsigned char ch, int col)
{
    if (ch == '\t') {
        return ((col / EDITOR_TAB_WIDTH) + 1) * EDITOR_TAB_WIDTH;
    }
    return col + 1;
}

/**
 * Lays out text as the view draws it, in rows of cols columns, from a row's start. Drawing the text, the cursor, the
 * selection, clicks and scrolling all go through it, so they agree on where every character is (#135).
 * - place() puts each character after the one before, as wide as columnAfter makes it. One that would cross its row's
 *   end starts the next row instead, unless it's the row's first, which is cut to fit.
 * - A newline ends its row. It takes a column, for the selection to show, if its row has one left, so a newline after
 *   a full row doesn't make a row of its own (#130).
 */
struct TextLayout {
    int cols;
    /* The character placed last: its row, the column it starts at, and the column after it */
    int row;
    int col;
    int end;
    bool newline;

    TextLayout(int columns) : cols(columns < 1 ? 1 : columns), row(0), col(0), end(0), newline(false) {}

    void place(unsigned char ch)
    {
        if (newline) {
            row++;
            col = 0;
        } else {
            col = end;
        }

        newline = ch == '\n';
        int next = newline ? col + 1 : columnAfter(ch, col);
        if (!newline && col > 0 && next > cols) {
            row++;
            col = 0;
            next = columnAfter(ch, 0);
        }
        end = minInt(next, cols);
    }
};

static int digitCount(int value)
{
    int v = value;
    int digits = 1;

    if (v < 0) {
        v = -v;
    }

    while (v >= 10) {
        v /= 10;
        digits++;
    }

    return digits;
}

/**
 * Writes path into out, which holds FS_PATH_MAX bytes, as an absolute path without empty, "." or ".." parts. A
 * relative path starts from the Editor's current directory, the one it was started in, as the kernel takes it. So
 * the status line and Save name the file the same way however it was typed.
 * @return 0, or -1 if the path is too long
 */
static int editor_absolute_path(const char* path, char* out)
{
    char joined[FS_PATH_MAX];
    int len = 0;
    if (path[0] != '/') {
        if (getcwd(joined, sizeof(joined)) < 0) {
            return -1;
        }
        len = strlen(joined);
        joined[len++] = '/';
    }

    int path_len = strlen(path);
    if (len + path_len >= (int)sizeof(joined)) {
        return -1;
    }
    memcpy(joined + len, path, path_len + 1);

    /* Copy each part, skipping empty ones and ".", and going back over the last part for ".." */
    int out_len = 0;
    const char* p = joined;
    while (*p != 0) {
        while (*p == '/') {
            p++;
        }
        if (*p == 0) {
            break;
        }

        const char* part = p;
        while (*p != 0 && *p != '/') {
            p++;
        }
        int part_len = p - part;

        if (part_len == 1 && part[0] == '.') {
            continue;
        }
        if (part_len == 2 && part[0] == '.' && part[1] == '.') {
            while (out_len > 0 && out[out_len - 1] != '/') {
                out_len--;
            }
            if (out_len > 0) {
                out_len--;
            }
            continue;
        }

        if (out_len + 1 + part_len >= FS_PATH_MAX) {
            return -1;
        }
        out[out_len++] = '/';
        memcpy(out + out_len, part, part_len);
        out_len += part_len;
    }

    if (out_len == 0) {
        out[out_len++] = '/';
    }
    out[out_len] = 0;
    return 0;
}

/* The longest window title, without its 0: GFX_MAX_WINDOW_NAME_SIZE in include/gfx/window.h, less one */
#define EDITOR_TITLE_MAX 19

/* The window title for a file: its name without its directory, shortened to fit a title */
static void editor_title(const char* path, char* out, int size)
{
    const char* name = path;
    for (const char* p = path; *p != 0; p++) {
        if (*p == '/' && p[1] != 0) {
            name = p + 1;
        }
    }

    int max = EDITOR_TITLE_MAX;
    if (max > size - 1) {
        max = size - 1;
    }

    int len = strlen(name);
    if (len <= max) {
        memcpy(out, name, len + 1);
        return;
    }
    memcpy(out, name, max - 3);
    memcpy(out + max - 3, "...", 4);
}

/* Opens path for saving: created if it doesn't exist, and emptied, so a shorter text leaves nothing of the old one */
static int open_editor_file_for_save(const char* path)
{
    if (path == nullptr || path[0] == 0) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    return open(path, FS_FILE_FLAG_CREATE | FS_FILE_FLAG_READ | FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE);
}

/**
 * Opens path to edit. A file that doesn't exist isn't created: the first save does that (#140).
 * @return the fd, or a negative error code from errors.h, -ERROR_FILE_NOT_FOUND for a file that isn't there
 */
static int open_editor_file(const char* path)
{
    if (path == nullptr || path[0] == 0) {
        return -ERROR_INVALID_ARGUMENTS;
    }

    return open(path, FS_FILE_FLAG_READ | FS_FILE_FLAG_WRITE);
}

enum {
    EDITOR_NEW_FILE_POPUP_ACTION_NONE = 0,
    EDITOR_NEW_FILE_POPUP_ACTION_CREATE = 1,
    EDITOR_NEW_FILE_POPUP_ACTION_CANCEL = 2
};

enum {
    EDITOR_NEW_FILE_POPUP_RESULT_CANCEL = 0,
    EDITOR_NEW_FILE_POPUP_RESULT_CREATE = 1,
    /* The window's widgets couldn't be made */
    EDITOR_NEW_FILE_POPUP_RESULT_ERROR = 2
};

struct EditorNewFilePopupShared {
    volatile int running;
    volatile int result;
    /* What the box holds when it opens, and the path typed, trimmed, when it closes with one */
    char path[FS_PATH_MAX];
};

static int trim_copy_path(const char* src, char* dst, int dst_size)
{
    if (src == nullptr || dst == nullptr || dst_size <= 1) {
        return 0;
    }

    int begin = 0;
    while (src[begin] == ' ' || src[begin] == '\t') {
        begin++;
    }

    int end = strlen(src);
    while (end > begin && (src[end - 1] == ' ' || src[end - 1] == '\t')) {
        end--;
    }

    int len = end - begin;
    if (len <= 0) {
        dst[0] = 0;
        return 0;
    }

    if (len >= dst_size) {
        len = dst_size - 1;
    }

    memcpy(dst, &src[begin], len);
    dst[len] = 0;
    return len;
}

static void __editor_new_file_popup_thread(void* arg)
{
    EditorNewFilePopupShared* shared = (EditorNewFilePopupShared*) arg;
    if (shared == nullptr) {
        return;
    }

    shared->result = EDITOR_NEW_FILE_POPUP_RESULT_CANCEL;

    Window popup(268, 112, "Save As", 1);

    volatile int action = EDITOR_NEW_FILE_POPUP_ACTION_NONE;
    WidgetManager* widgets = nullptr;
    Input* path_input = nullptr;

    char status[96];
    memset(status, 0, sizeof(status));

    char input_placeholder[] = "/path/to/file.c";
    char input_tag[] = "new_path";
    char create_label[] = "Save";
    char cancel_label[] = "Cancel";

    auto freeWidgets = [&]() {
        if (widgets != nullptr) {
            delete widgets;
            widgets = nullptr;
            path_input = nullptr;
        }
    };

    auto buildWidgets = [&]() -> int {
        freeWidgets();
        action = EDITOR_NEW_FILE_POPUP_ACTION_NONE;

        widgets = new WidgetManager();
        if (widgets == nullptr) {
            return -1;
        }

        Layout* input_row = new Layout(10, 32, 248, 18, HORIZONTAL, LAYOUT_FLAG_NONE);
        if (input_row == nullptr) {
            freeWidgets();
            return -1;
        }

        path_input = new Input(244, 14, input_placeholder, input_tag);
        if (path_input == nullptr) {
            delete input_row;
            freeWidgets();
            return -1;
        }

        if (input_row->addWidget(path_input, LEFT) < 0) {
            delete input_row;
            freeWidgets();
            return -1;
        }

        if (widgets->addLayout(input_row) < 0) {
            delete input_row;
            freeWidgets();
            return -1;
        }

        Layout* buttons = new Layout(48, 76, 172, 18, HORIZONTAL, LAYOUT_FLAG_NONE);
        if (buttons == nullptr) {
            freeWidgets();
            return -1;
        }

        Button* create_button = new Button(80, 14, create_label, Function<void()>([&action]() {
            action = EDITOR_NEW_FILE_POPUP_ACTION_CREATE;
        }));
        Button* cancel_button = new Button(80, 14, cancel_label, Function<void()>([&action]() {
            action = EDITOR_NEW_FILE_POPUP_ACTION_CANCEL;
        }));
        if (create_button == nullptr || cancel_button == nullptr) {
            if (create_button != nullptr) {
                delete create_button;
            }
            if (cancel_button != nullptr) {
                delete cancel_button;
            }
            delete buttons;
            freeWidgets();
            return -1;
        }

        if (buttons->addWidget(create_button, LEFT) < 0 ||
            buttons->addWidget(cancel_button, LEFT) < 0) {
            delete buttons;
            freeWidgets();
            return -1;
        }

        if (widgets->addLayout(buttons) < 0) {
            delete buttons;
            freeWidgets();
            return -1;
        }

        return 0;
    };

    if (buildWidgets() < 0) {
        shared->result = EDITOR_NEW_FILE_POPUP_RESULT_ERROR;
        shared->running = 0;
        return;
    }

    for (const char* p = shared->path; *p != 0; p++) {
        path_input->Keyboard(*p);
    }

    while (1) {
        popup.drawRect(0, 0, 268, 112, COLOR_BG);
        popup.drawContouredRect(0, 0, 268, 112);
        popup.drawText(10, 8, "Save File As", COLOR_BLACK);
        popup.drawText(10, 20, "Path:", COLOR_BLACK);
        popup.drawText(10, 56, "Enter=Save  Esc=Cancel", COLOR_VGA_MEDIUM_DARK_GRAY);

        if (status[0] != 0) {
            popup.drawText(10, 66, status, COLOR_VGA_RED);
        }

        if (widgets != nullptr) {
            widgets->draw(&popup);
        }

        struct gfx_event event;
        int ret = gfx_get_event(&event, GFX_EVENT_BLOCKING);
        if (ret < 0) {
            continue;
        }

        switch (event.event) {
        case GFX_EVENT_KEYBOARD:
            if (event.data == '\n' || event.data == '\r') {
                action = EDITOR_NEW_FILE_POPUP_ACTION_CREATE;
                break;
            }

            if (event.data == 27) {
                action = EDITOR_NEW_FILE_POPUP_ACTION_CANCEL;
                break;
            }

            if (path_input != nullptr) {
                unsigned char key = (unsigned char) event.data;
                if (key == 127) {
                    key = '\b';
                }
                path_input->Keyboard(key);
            }
            break;

        case GFX_EVENT_MOUSE:
            if (widgets != nullptr) {
                widgets->Mouse(event.data, event.data2);
            }
            break;

        case GFX_EVENT_EXIT:
            action = EDITOR_NEW_FILE_POPUP_ACTION_CANCEL;
            break;

        case GFX_EVENT_RESOLUTION:
            break;

        default:
            break;
        }

        if (action == EDITOR_NEW_FILE_POPUP_ACTION_CREATE) {
            action = EDITOR_NEW_FILE_POPUP_ACTION_NONE;
            memset(status, 0, sizeof(status));

            char trimmed[FS_PATH_MAX];
            memset(trimmed, 0, sizeof(trimmed));
            if (path_input == nullptr || trim_copy_path(path_input->getData(), trimmed, sizeof(trimmed)) <= 0) {
                strncpy(status, "Path is empty.", (uint32_t)(sizeof(status) - 1));
                continue;
            }

            strncpy(shared->path, trimmed, (uint32_t)(sizeof(shared->path) - 1));
            shared->path[sizeof(shared->path) - 1] = 0;
            shared->result = EDITOR_NEW_FILE_POPUP_RESULT_CREATE;
            shared->running = 0;
            freeWidgets();
            return;
        }

        if (action == EDITOR_NEW_FILE_POPUP_ACTION_CANCEL) {
            shared->result = EDITOR_NEW_FILE_POPUP_RESULT_CANCEL;
            shared->running = 0;
            freeWidgets();
            return;
        }
    }
}

/* Each header button's icon, in /sysutil/icons, and the key it presses, by EDITOR_BUTTON_* */
static const char* const editor_button_icons[EDITOR_BUTTON_COUNT] = {"new_16", "save_16", "help_16"};
static const unsigned char editor_button_keys[EDITOR_BUTTON_COUNT] = {CTRLN, CTRLS, CTRLH};

Editor::Editor()
    : Window(EDITOR_DEFAULT_WINDOW_WIDTH, EDITOR_DEFAULT_HEIGHT, "Editor", 1)
{
    treeView = nullptr;
    m_fd = -1;
    m_textBuffer = nullptr;
    m_bufferSize = EDITOR_BUFFER_SIZE;
    m_fileSize = 0;
    m_bufferHead = 0;
    m_bufferEdit = 0;
    m_dirty = 0;
    m_readOnly = false;
    m_autoBraceCount = 0;
    m_undo = nullptr;
    m_undoing = false;
    m_undoKey = EDITOR_UNDO_CLOSED;
    m_undoCursor = 0;
    m_undoAnchor = -1;
    undoClear();
    m_preferredColumn = -1;
    m_selAnchor = -1;
    m_dragging = false;
    m_dragScroll = 0;
    m_dragX = 0;
    m_pressedButton = EDITOR_BUTTON_NONE;
    m_pressedShown = false;
    m_treeWidth = TREE_VIEW_WIDTH;
    m_windowWidth = EDITOR_DEFAULT_WINDOW_WIDTH;
    m_treeCollapsed = false;
    m_treeOpenWidth = TREE_VIEW_WIDTH;
    m_resizingTree = false;
    m_treeGrabX = 0;
    m_togglePressed = false;
    m_toggleShown = false;
    m_mouseHeld = false;
    m_pressOnTree = false;
    m_keepStatus = 0;
    m_prompt = EDITOR_PROMPT_NONE;
    m_promptText[0] = 0;
    m_promptLen = 0;
    m_promptFilled = false;
    m_findText[0] = 0;
    m_findIndex = 0;
    m_findCount = -1;
    scrollY = 0;
    m_scrollWrap = 0;
    m_textColor = COLOR_TEXT;
    c_width = EDITOR_DEFAULT_RIGHT_PANE_WIDTH;
    c_height = EDITOR_DEFAULT_HEIGHT;

    m_textBuffer = (unsigned char*) malloc(m_bufferSize);
    if (m_textBuffer == nullptr) {
        exit();
    }

    memset(m_textBuffer, 0, m_bufferSize);
    memset(m_currentPath, 0, sizeof(m_currentPath));

    treeView = new TreeView(0, 0, m_treeWidth, c_height);
    if (treeView != nullptr) {
        treeView->setScrollable(true);
        treeView->setBox(EDITOR_TREE_TOGGLE_X, EDITOR_TREE_BOX_TOP, false);
    }

    /* A file that can't be read gives a placeholder, so no button is ever blank */
    for (int button = 0; button < EDITOR_BUTTON_COUNT; button++) {
        gfx_load_icon(editor_button_icons[button], &m_buttonIcons[button]);
    }

    gfx_draw_rectangle(0, 0, EDITOR_DEFAULT_WINDOW_WIDTH, c_height, COLOR_BG);
    gfx_draw_format_text(20, 20, COLOR_VGA_MEDIUM_GRAY, "Loading...");

    reDrawHeader();
    reDraw(0, 0);
}

Editor::~Editor()
{
    if (m_fd >= 0) {
        fclose(m_fd);
        m_fd = -1;
    }

    if (treeView != nullptr) {
        delete treeView;
        treeView = nullptr;
    }

    for (int button = 0; button < EDITOR_BUTTON_COUNT; button++) {
        gfx_free_icon(&m_buttonIcons[button]);
    }

    undoClear();

    if (m_textBuffer != nullptr) {
        free(m_textBuffer);
        m_textBuffer = nullptr;
    }
}

int Editor::gutterWidth() const
{
    int digits = digitCount(countLines());
    int width = (digits * 8) + 6;

    if (width < 18) {
        width = 18;
    }
    if (width > 42) {
        width = 42;
    }

    return width;
}

int Editor::textStartX() const
{
    return m_treeWidth + gutterWidth() + TEXT_PADDING;
}

int Editor::textClipWidth() const
{
    int width = c_width - gutterWidth() - (2 * TEXT_PADDING);
    if (width < 8) {
        width = 8;
    }
    return width;
}

int Editor::visibleLines() const
{
    int h = c_height - TEXT_TOP - TEXT_PADDING - STATUS_HEIGHT;
    if (h < TEXT_ROW_HEIGHT) {
        return 1;
    }

    return h / TEXT_ROW_HEIGHT;
}

int Editor::visibleColumns() const
{
    /* Short of the scroll bar's strip, whether or not the bar is shown */
    int w = textClipWidth() - SCROLLBAR_WIDTH;
    if (w < 8) {
        return 1;
    }

    return w / 8;
}

int Editor::lineNumberAtIndex(int index) const
{
    int line = 1;
    int i = clampInt(index, 0, m_bufferHead);

    for (int j = 0; j < i; j++) {
        if (m_textBuffer[j] == '\n') {
            line++;
        }
    }

    return line;
}

int Editor::indexFromLine(int line) const
{
    if (line <= 0) {
        return 0;
    }

    int currentLine = 0;
    int i = 0;
    while (i < m_bufferHead && currentLine < line) {
        if (m_textBuffer[i] == '\n') {
            currentLine++;
        }
        i++;
    }

    return i;
}

int Editor::lineStart(int index) const
{
    int i = clampInt(index, 0, m_bufferHead);

    while (i > 0 && m_textBuffer[i - 1] != '\n') {
        i--;
    }

    return i;
}

int Editor::lineEnd(int index) const
{
    int i = clampInt(index, 0, m_bufferHead);

    while (i < m_bufferHead && m_textBuffer[i] != '\n' && m_textBuffer[i] != 0) {
        i++;
    }

    return i;
}

int Editor::lineRowsFromStart(int line_start, int cols) const
{
    int line_end = lineEnd(line_start);
    int rows = 1;

    /* A row that starts before the line's end is the line's too */
    int row_start = line_start;
    int next = nextVisualRowStart(row_start, cols);
    while (next > row_start && next < line_end) {
        rows++;
        row_start = next;
        next = nextVisualRowStart(row_start, cols);
    }

    return rows;
}

/* The column index is at in its line, counted from 0 with the line laid out in one row, so a tab ends at a tab stop */
int Editor::lineColumn(int index) const
{
    int limit = clampInt(index, 0, m_bufferHead);
    int col = 0;

    for (int i = lineStart(limit); i < limit; i++) {
        col = columnAfter(m_textBuffer[i], col);
    }

    return col;
}

/**
 * The boundary between characters in the line from line_start that's nearest column, as lineColumn counts them: in a
 * tab, whichever of its sides is nearer, the one before it if they're as near. A shorter line's end
 */
int Editor::indexAtColumn(int line_start, int column) const
{
    int i = clampInt(line_start, 0, m_bufferHead);
    int col = 0;

    while (i < m_bufferHead && m_textBuffer[i] != '\n' && col < column) {
        int next = columnAfter(m_textBuffer[i], col);
        if (next > column && column - col <= next - column) {
            break;
        }
        col = next;
        i++;
    }

    return i;
}

int Editor::viewStartIndex(int cols) const
{
    int idx = indexFromLine(scrollY);

    for (int r = 0; r < m_scrollWrap; r++) {
        int next = nextVisualRowStart(idx, cols);
        if (next <= idx) {
            break;
        }
        idx = next;
    }

    return idx;
}

int Editor::nextVisualRowStart(int row_start_index, int cols) const
{
    TextLayout at(cols);
    for (int i = clampInt(row_start_index, 0, m_bufferHead); i < m_bufferHead; i++) {
        /* The first character on the next row, after a newline or as one that doesn't fit on this one */
        at.place(m_textBuffer[i]);
        if (at.row > 0) {
            return i;
        }
    }

    return m_bufferHead;
}

int Editor::prevVisualRowStart(int row_start_index, int cols) const
{
    int target = clampInt(row_start_index, 0, m_bufferHead);
    if (target <= 0) {
        return 0;
    }

    /* The last row that starts before target: the one whose next row is target, or would be past it */
    int cur = 0;

    while (cur < target) {
        int next = nextVisualRowStart(cur, cols);
        if (next <= cur || next >= target) {
            break;
        }

        cur = next;
    }

    return cur;
}

/* The rows the text takes from the row that starts at index to its end, counted as scrolling counts them */
int Editor::rowsFrom(int index, int cols) const
{
    int rows = 0;
    int i = clampInt(index, 0, m_bufferHead);
    while (i < m_bufferHead) {
        int next = nextVisualRowStart(i, cols);
        if (next <= i) {
            break;
        }
        rows++;
        i = next;
    }

    /* The empty line after a newline at the end has a row too */
    if (m_bufferHead == 0 || m_textBuffer[m_bufferHead - 1] == '\n') {
        rows++;
    }
    return rows;
}

void Editor::cursorFromViewStart(int view_start, int cols, int* out_row, int* out_col) const
{
    placeFromViewStart(m_bufferEdit, view_start, cols, out_row, out_col);
}

/* Where the cursor would be drawn at index: its row from the view's start, -1 if it's above, and its column */
void Editor::placeFromViewStart(int index, int view_start, int cols, int* out_row, int* out_col) const
{
    if (index < view_start) {
        *out_row = -1;
        *out_col = 0;
        return;
    }

    TextLayout at(cols);
    int limit = clampInt(index, 0, m_bufferHead);
    for (int i = view_start; i < limit; i++) {
        at.place(m_textBuffer[i]);
    }

    /**
     * The cursor is where the character after it is, which after a full row, or before a tab that doesn't fit, is the
     * next row's start. A newline, or the end of the text, stays on the row, so after a full row the cursor is past
     * its end, at column cols, where reDraw draws it in the right margin
     */
    at.place(limit < m_bufferHead ? m_textBuffer[limit] : '\n');

    *out_row = at.row;
    *out_col = at.col;
}

void Editor::setScrollFromIndex(int index, int cols)
{
    int idx = clampInt(index, 0, m_bufferHead);

    int line = 0;
    int line_start = 0;
    for (int i = 0; i < idx; i++) {
        if (m_textBuffer[i] == '\n') {
            line++;
            line_start = i + 1;
        }
    }

    scrollY = line;
    m_scrollWrap = 0;

    int row_start = line_start;
    while (row_start < idx) {
        int next = nextVisualRowStart(row_start, cols);
        if (next <= row_start || next > idx) {
            break;
        }

        m_scrollWrap++;
        row_start = next;
    }

    normalizeScroll(cols);
}

void Editor::normalizeScroll(int cols)
{
    if (cols < 1) {
        cols = 1;
    }

    int totalLines = countLines();
    if (totalLines < 1) {
        totalLines = 1;
    }

    scrollY = clampInt(scrollY, 0, totalLines - 1);

    int line_start = indexFromLine(scrollY);
    int line_rows = lineRowsFromStart(line_start, cols);
    m_scrollWrap = clampInt(m_scrollWrap, 0, line_rows - 1);
}

void Editor::ensureCursorVisible(int cols, int rows)
{
    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }

    normalizeScroll(cols);

    int guard_max = m_bufferHead + rows + 16;
    if (guard_max < 64) {
        guard_max = 64;
    }

    for (int guard = 0; guard < guard_max; guard++) {
        int view_start = viewStartIndex(cols);
        int cursor_row = 0;
        int cursor_col = 0;
        cursorFromViewStart(view_start, cols, &cursor_row, &cursor_col);

        if (cursor_row < 0) {
            int prev = prevVisualRowStart(view_start, cols);
            if (prev == view_start) {
                break;
            }
            setScrollFromIndex(prev, cols);
            continue;
        }

        if (cursor_row >= rows) {
            int next = nextVisualRowStart(view_start, cols);
            if (next == view_start) {
                break;
            }
            setScrollFromIndex(next, cols);
            continue;
        }

        break;
    }

    normalizeScroll(cols);
}

void Editor::drawLineNumbers(const int* row_lines, int row_count)
{
    int gutter_x = m_treeWidth;
    int gutter_w = gutterWidth();

    int text_h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;
    if (text_h < 8) {
        text_h = 8;
    }

    gfx_draw_rectangle(gutter_x, HEADER_HEIGHT, gutter_w, text_h, COLOR_BG);
    /**
     * From below the header, which isn't redrawn when the gutter widens, so the line would stay there at its old place,
     * to the status line's band, which the status line's field continues below
     */
    gfx_draw_line(gutter_x + gutter_w - 1, HEADER_HEIGHT, gutter_x + gutter_w - 1, c_height - STATUS_HEIGHT - 1, COLOR_VGA_MEDIUM_GRAY);

    int rows = visibleLines();
    int draw_rows = minInt(rows, row_count);
    for (int row = 0; row < draw_rows; row++) {
        int line_no = row_lines[row];
        if (line_no <= 0) {
            continue;
        }

        if (row > 0 && row_lines[row - 1] == line_no) {
            continue;
        }

        int digits = digitCount(line_no);
        int x = gutter_x + gutter_w - (digits * 8) - 2;
        if (x < gutter_x + 1) {
            x = gutter_x + 1;
        }

        gfx_draw_format_text(x, TEXT_TOP + (row * TEXT_ROW_HEIGHT) + TEXT_CHAR_Y, COLOR_VGA_MEDIUM_GRAY, "%d", line_no);
    }
}

void Editor::drawStatusLine(color_t color, const char* fmt, ...)
{
    /* csprintf writes up to 1,023 characters and a 0. Static, to keep it off the app's 8 KB stack. */
    static char msg[1024];
    memset(msg, 0, sizeof(msg));

    va_list args;
    va_start(args, fmt);
    csprintf(msg, fmt, args);
    va_end(args);

    int x = textStartX();
    int y = c_height - STATUS_HEIGHT;
    int w = textClipWidth();

    if (w < 8) {
        w = 8;
    }

    /**
     * The whole band, under the gutter too, since the text starts further right as the gutter widens, and its old
     * start would stay behind (#144). Then the field, sunken: a shadow along its top and left, and the light edge
     * along its bottom and right. It starts below the gutter's line, which drawLineNumbers stops above the band
     */
    gfx_draw_rectangle(m_treeWidth, y, c_width, STATUS_HEIGHT, COLOR_BAR_FACE);

    int field_x = x - TEXT_PADDING - 1;
    int field_y = y + EDITOR_STATUS_MARGIN;
    int field_w = m_treeWidth + c_width - EDITOR_STATUS_MARGIN - field_x;
    int field_h = STATUS_HEIGHT - (2 * EDITOR_STATUS_MARGIN);
    gfx_draw_rectangle(field_x, field_y, field_w, 1, COLOR_BAR_SHADOW);
    gfx_draw_rectangle(field_x, field_y, 1, field_h, COLOR_BAR_SHADOW);
    gfx_draw_rectangle(field_x + 1, field_y + field_h - 1, field_w - 1, 1, COLOR_BAR_LIGHT);
    gfx_draw_rectangle(field_x + field_w - 1, field_y + 1, 1, field_h - 1, COLOR_BAR_LIGHT);

    int max_chars = w / 8;
    if (max_chars < 1) {
        return;
    }

    int len = strlen(msg);
    if (len > max_chars) {
        msg[max_chars] = 0;
    }

    /* Inside the field's edge and a pixel of space */
    gfx_draw_format_text(x, field_y + 2, color, "%s", msg);
    m_keepStatus = color == COLOR_VGA_RED;
}

/**
 * Opens a prompt on the status line, EDITOR_PROMPT_*, which takes every key until it's closed (promptKey). Find's is
 * filled in with what's selected, if it's on one line and fits, or else the last search, and the first key typed
 * replaces that
 */
void Editor::openPrompt(int kind)
{
    m_prompt = kind;
    m_promptText[0] = 0;
    m_promptLen = 0;
    m_promptFilled = false;
    m_findCount = -1;

    if (kind == EDITOR_PROMPT_FIND) {
        int start = 0;
        int end = 0;
        bool fits = selectionRange(start, end) && end - start <= EDITOR_PROMPT_MAX;
        for (int i = start; fits && i < end; i++) {
            fits = m_textBuffer[i] >= 32 && m_textBuffer[i] <= 126;
        }

        const char* fill = m_findText;
        int fill_len = strlen(m_findText);
        if (fits) {
            fill = (const char*)&m_textBuffer[start];
            fill_len = end - start;
        }
        memcpy(m_promptText, fill, fill_len);
        m_promptText[fill_len] = 0;
        m_promptLen = fill_len;
        m_promptFilled = fill_len > 0;
    }

    drawStatus();
}

/* Closes the prompt. The status line is the file's again at its next draw */
void Editor::closePrompt()
{
    m_prompt = EDITOR_PROMPT_NONE;
    m_promptText[0] = 0;
    m_promptLen = 0;
    m_promptFilled = false;
    m_findCount = -1;
}

/**
 * The prompt: its label, what's typed, and a block after that for the cursor, then for Find, after the last search,
 * which match is selected, or "Not found" in red.
 * - The line number's prompt shows the lines there are, unless that leaves no room for the cursor, when it's only "Line:".
 * - Find's text, when it doesn't fit, shows its end after "...", since that's where typing goes. The count goes first
 *   to make room, unless that leaves less than 4 characters of the text. A line number is never cut short that way:
 *   all its digits are worth more than the cursor.
 * - Find's filled-in text is drawn highlighted, as selected text is, since the first key typed replaces it.
 */
void Editor::drawPrompt()
{
    /* Static, as drawStatusLine's is */
    static char label[32];
    static char shown[EDITOR_PROMPT_MAX + 4];
    static char count[32];

    int max_chars = textClipWidth() / 8;
    const char* suffix = "";
    color_t suffix_color = COLOR_VGA_MEDIUM_DARK_GRAY;

    if (m_prompt == EDITOR_PROMPT_LINE) {
        int len = sprintf(label, "Go to line (1-%d): ", countLines());
        if (len + m_promptLen + 1 > max_chars) {
            sprintf(label, "Line: ");
        }
    } else {
        sprintf(label, "Find: ");
        if (m_findCount == 0) {
            suffix = "Not found";
            suffix_color = COLOR_VGA_RED;
        } else if (m_findCount > 0) {
            sprintf(count, "%d of %d", m_findIndex, m_findCount);
            suffix = count;
        }
    }

    int label_len = strlen(label);
    int suffix_len = strlen(suffix);
    /* Room for what's typed, with the cursor after it, and with the suffix after that and a space */
    int room = max_chars - label_len - 1;
    if (suffix_len > 0) {
        if (room - (suffix_len + 1) >= minInt(m_promptLen, 4)) {
            room -= suffix_len + 1;
        } else {
            suffix_len = 0;
        }
    }

    if (m_prompt == EDITOR_PROMPT_FIND && m_promptLen > room && room > 3) {
        sprintf(shown, "...%s", m_promptText + m_promptLen - (room - 3));
    } else {
        sprintf(shown, "%s", m_promptText);
    }
    /* Only what fits in the field, as drawStatusLine would cut it, so the highlight covers what's shown */
    int shown_len = strlen(shown);
    int fit = max_chars - label_len > 0 ? max_chars - label_len : 0;
    if (shown_len > fit) {
        shown_len = fit;
        shown[shown_len] = 0;
    }

    drawStatusLine(COLOR_BLACK, "%s%s", label, shown);

    /* Where drawStatusLine draws the text */
    int x = textStartX();
    int y = c_height - STATUS_HEIGHT + EDITOR_STATUS_MARGIN + 2;
    int end = label_len + shown_len;

    if (m_promptFilled && shown_len > 0) {
        gfx_draw_rectangle(x + (label_len * 8), y - 1, shown_len * 8, 10, COLOR_SELECTED);
        gfx_draw_format_text(x + (label_len * 8), y, COLOR_BLACK, "%s", shown);
    }

    if (end < max_chars) {
        gfx_draw_rectangle(x + (end * 8), y, 8, 8, COLOR_VGA_MEDIUM_GRAY);
    }

    if (suffix_len > 0 && end + 2 + suffix_len <= max_chars) {
        gfx_draw_format_text(x + ((end + 2) * 8), y, suffix_color, "%s", suffix);
    }
}

/**
 * A key typed while a prompt is open, with the modifier keys held. Every key that isn't the prompt's does nothing, so
 * no key meant for the prompt can change the text behind it.
 * - Esc, or the key that opened the prompt, closes it.
 * - Enter with nothing typed closes it. Ctrl+G's goes to the line typed and closes. Find's selects the next match, or
 *   with Shift the one before, and stays open (findNext).
 * - Backspace deletes what was typed last, or all of what was filled in.
 * - A line number takes up to EDITOR_LINE_DIGITS digits, and Find up to EDITOR_PROMPT_MAX characters from 32 to 126.
 *   The first replaces what was filled in.
 */
void Editor::promptKey(unsigned char c, int modifiers)
{
    unsigned char own = m_prompt == EDITOR_PROMPT_FIND ? CTRLF : CTRLG;
    if (c == 27 || c == own) {
        closePrompt();
        drawStatus();
        return;
    }

    if (c == '\n' || c == '\r') {
        if (m_promptLen == 0) {
            closePrompt();
            drawStatus();
            return;
        }

        if (m_prompt == EDITOR_PROMPT_FIND) {
            findNext((modifiers & KEY_MOD_SHIFT) != 0);
            reDraw(0, m_bufferHead);
            return;
        }

        int number = 0;
        for (int i = 0; i < m_promptLen; i++) {
            number = (number * 10) + (m_promptText[i] - '0');
        }
        closePrompt();
        goToLine(number);
        reDraw(0, m_bufferHead);
        return;
    }

    if (c == '\b') {
        if (m_promptLen == 0) {
            return;
        }
        m_promptLen = m_promptFilled ? 0 : m_promptLen - 1;
        m_promptText[m_promptLen] = 0;
        m_promptFilled = false;
        m_findCount = -1;
        drawStatus();
        return;
    }

    if (c < 32 || c > 126 || (m_prompt == EDITOR_PROMPT_LINE && (c < '0' || c > '9'))) {
        return;
    }

    if (m_promptFilled) {
        m_promptLen = 0;
        m_promptFilled = false;
    }
    if (m_promptLen >= (m_prompt == EDITOR_PROMPT_LINE ? EDITOR_LINE_DIGITS : EDITOR_PROMPT_MAX)) {
        return;
    }

    m_promptText[m_promptLen++] = (char)c;
    m_promptText[m_promptLen] = 0;
    m_findCount = -1;
    drawStatus();
}

/* Whether the len characters typed at the prompt are in the text at index, case included */
bool Editor::matchAt(int index, int len) const
{
    if (index < 0 || len <= 0 || index + len > m_bufferHead) {
        return false;
    }
    return m_textBuffer[index] == (unsigned char)m_promptText[0] && memcmp(&m_textBuffer[index], m_promptText, len) == 0;
}

/**
 * Selects the next match of what's typed at the Find prompt, or with backward the one before, going round the text's
 * end, and shows it (#137). The cursor goes at its end, as after selecting it with Shift+Right.
 * - The next starts after the selection's start, so Enter again steps past the match selected, or at the cursor
 *   without one. The one before starts before the selection's start, or the cursor.
 * - Every place the text has it counts, overlapping ones too, so the steps and the count, m_findIndex of
 *   m_findCount, agree. With none, m_findCount is 0 and the selection stays as it is.
 * - What's searched for is kept, for the next Ctrl+F without a selection.
 */
void Editor::findNext(bool backward)
{
    int len = m_promptLen;
    memcpy(m_findText, m_promptText, len + 1);

    int start = 0;
    int end = 0;
    bool selected = selectionRange(start, end);
    int last = m_bufferHead - len;
    int found = -1;

    if (backward) {
        int from = selected ? start : m_bufferEdit;
        for (int i = minInt(from - 1, last); i >= 0 && found < 0; i--) {
            found = matchAt(i, len) ? i : -1;
        }
        for (int i = last; i >= from && found < 0; i--) {
            found = matchAt(i, len) ? i : -1;
        }
    } else {
        int from = selected ? start + 1 : m_bufferEdit;
        for (int i = from; i <= last && found < 0; i++) {
            found = matchAt(i, len) ? i : -1;
        }
        for (int i = 0; i < from && i <= last && found < 0; i++) {
            found = matchAt(i, len) ? i : -1;
        }
    }

    m_findCount = 0;
    m_findIndex = 0;
    for (int i = 0; i <= last; i++) {
        if (matchAt(i, len)) {
            m_findCount++;
            m_findIndex += i <= found;
        }
    }
    if (found < 0) {
        return;
    }

    m_selAnchor = found;
    m_bufferEdit = found + len;
    m_preferredColumn = -1;
    revealRange(found, found + len);
}

/**
 * Moves the cursor to the start of line, counted from 1 as the gutter counts them, and clamped to the lines there are
 * (#137). It clears the selection, and shows the line as revealRange does
 */
void Editor::goToLine(int line)
{
    line = clampInt(line, 1, countLines());
    m_bufferEdit = indexFromLine(line - 1);
    m_preferredColumn = -1;
    clearSelection();
    revealRange(m_bufferEdit, m_bufferEdit);
}

/**
 * Scrolls so that the text from start to end is in view, with the cursor there. If it's in view already, nothing
 * moves. Otherwise start's row goes in the middle, or lower, so the view doesn't run past the text's end when there's
 * text above to show instead
 */
void Editor::revealRange(int start, int end)
{
    int cols = visibleColumns();
    int rows = visibleLines();
    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }
    normalizeScroll(cols);

    int view_start = viewStartIndex(cols);
    int start_row = 0;
    int end_row = 0;
    int col = 0;
    placeFromViewStart(start, view_start, cols, &start_row, &col);
    placeFromViewStart(end, view_start, cols, &end_row, &col);
    if (start_row >= 0 && start_row < rows && end_row >= 0 && end_row < rows) {
        return;
    }

    /* Start's row at the top, then half a screen of rows above it, then as many more as the view would show past the end */
    setScrollFromIndex(start, cols);
    scroll(-(rows / 2));
    int below = rowsFrom(viewStartIndex(cols), cols);
    if (below < rows) {
        scroll(below - rows);
    }
    ensureCursorVisible(cols, rows);
}

/* The bar above the text, drawn as Graphics.hpp's drawHeaderTable draws its shorter one, with the buttons */
void Editor::reDrawHeader()
{
    int x = m_treeWidth;
    int w = c_width;
    int face = HEADER_HEIGHT - 3;

    gfx_draw_rectangle(x, 0, w, HEADER_HEIGHT, COLOR_BG);
    gfx_draw_rectangle(x, 0, w, face, COLOR_BAR_FACE);
    gfx_draw_rectangle(x + 1, 1, w - 1, 1, COLOR_BAR_LIGHT);
    gfx_draw_rectangle(x + 1, 1, 1, face, COLOR_BAR_LIGHT);
    gfx_draw_rectangle(x + w - 1, 1, 1, face, COLOR_BAR_SHADOW);
    gfx_draw_rectangle(x + 1, face, w - 1, 1, COLOR_BAR_SHADOW);
    gfx_draw_rectangle(x + 1, face + 1, w - 1, 1, COLOR_BLACK);
    gfx_draw_rectangle(x + 1, face + 2, w - 1, 1, COLOR_BAR_LIGHT);

    for (int button = 0; button < EDITOR_BUTTON_COUNT; button++) {
        drawButton(button, button == m_pressedButton && m_pressedShown);
    }
}

/**
 * Where a header button goes across the window: New and Save from the bar's left, and Help at its right, with New and
 * Help EDITOR_BUTTON_MARGIN in from the light line or shadow at that end. @return false for Help when the bar is too
 * narrow for it beside Save
 */
bool Editor::buttonPosition(int button, int* x) const
{
    int left = m_treeWidth + 1 + EDITOR_BUTTON_MARGIN;

    switch (button) {
    case EDITOR_BUTTON_NEW:
        *x = left;
        return true;

    case EDITOR_BUTTON_SAVE:
        *x = left + EDITOR_BUTTON_SIZE + EDITOR_BUTTON_GAP;
        return true;

    case EDITOR_BUTTON_HELP:
        *x = m_treeWidth + c_width - EDITOR_BUTTON_MARGIN - EDITOR_BUTTON_SIZE;
        return *x >= left + 2 * (EDITOR_BUTTON_SIZE + EDITOR_BUTTON_GAP);

    default:
        return false;
    }
}

/* The header button at x, y in the window, or EDITOR_BUTTON_NONE */
int Editor::buttonAt(int x, int y) const
{
    if (y < EDITOR_BUTTON_Y || y >= EDITOR_BUTTON_Y + EDITOR_BUTTON_SIZE) {
        return EDITOR_BUTTON_NONE;
    }

    for (int button = 0; button < EDITOR_BUTTON_COUNT; button++) {
        int left = 0;
        if (buttonPosition(button, &left) && x >= left && x < left + EDITOR_BUTTON_SIZE) {
            return button;
        }
    }
    return EDITOR_BUTTON_NONE;
}

/* Draws a header button raised, or pushed in while it's pressed, with its icon in the middle */
void Editor::drawButton(int button, bool pressed)
{
    int x = 0;
    if (!buttonPosition(button, &x)) {
        return;
    }

    int y = EDITOR_BUTTON_Y;
    int size = EDITOR_BUTTON_SIZE;
    gfx_draw_bevel(x, y, size, size, pressed);

    /* A pixel down and right when pressed, as if pushed in */
    const struct image* icon = &m_buttonIcons[button];
    int shift = pressed ? 1 : 0;
    drawImage(x + (size - icon->width) / 2 + shift, y + (size - icon->height) / 2 + shift, icon);
}

/**
 * Presses, drags and releases for the header's buttons. A press on one draws it pushed in while the mouse stays over it,
 * and a release over it clicks it.
 * @return whether the event was the buttons', not the text's. clicked gets the button clicked, or EDITOR_BUTTON_NONE
 */
bool Editor::buttonMouse(const struct gfx_event& event, int* clicked)
{
    int x = (int16_t)event.data;
    int y = (int16_t)event.data2;
    *clicked = EDITOR_BUTTON_NONE;

    if (event.event == GFX_EVENT_MOUSE_DOWN) {
        m_pressedButton = buttonAt(x, y);
        if (m_pressedButton == EDITOR_BUTTON_NONE) {
            return false;
        }
        m_pressedShown = true;
        drawButton(m_pressedButton, true);
        return true;
    }

    if (m_pressedButton == EDITOR_BUTTON_NONE) {
        return false;
    }

    bool over = buttonAt(x, y) == m_pressedButton;
    if (event.event == GFX_EVENT_MOUSE_UP) {
        if (over) {
            *clicked = m_pressedButton;
        }
        drawButton(m_pressedButton, false);
        m_pressedButton = EDITOR_BUTTON_NONE;
        m_pressedShown = false;
    } else if (over != m_pressedShown) {
        m_pressedShown = over;
        drawButton(m_pressedButton, over);
    }
    return true;
}

/**
 * The tree, the grip on its right border, three short lines in the middle of the gray between its box and its edge, and
 * its toggle. Collapsed, it's a bar of the same gray with the toggle at its top
 */
void Editor::drawTreePane()
{
    if (m_treeCollapsed) {
        drawContouredRect(0, 0, m_treeWidth, c_height);
    } else {
        treeView->drawTree(this, m_currentPath);

        int y = (c_height - EDITOR_TREE_GRIP_LENGTH) / 2;
        for (int line = 0; line < 3; line++) {
            gfx_draw_rectangle(m_treeWidth - 7 + (line * 2), y, 1, EDITOR_TREE_GRIP_LENGTH, COLOR_BAR_SHADOW);
        }
    }

    drawTreeToggle(m_togglePressed && m_toggleShown);
}

/* width, kept between EDITOR_TREE_MIN_WIDTH and what leaves the pane right of the tree its least, EDITOR_MIN_RIGHT_PANE_WIDTH */
int Editor::clampTreeWidth(int width) const
{
    int widest = m_windowWidth - EDITOR_MIN_RIGHT_PANE_WIDTH;
    if (width > widest) {
        width = widest;
    }
    if (width < EDITOR_TREE_MIN_WIDTH) {
        width = EDITOR_TREE_MIN_WIDTH;
    }
    return width;
}

/* Makes the tree width wide, as far as it can go, and the pane right of it the rest of the window, and redraws them */
void Editor::setTreeWidth(int width)
{
    width = clampTreeWidth(width);
    if (width == m_treeWidth) {
        return;
    }

    m_treeWidth = width;
    applyResolution(m_windowWidth, c_height);

    drawTreePane();
    reDrawHeader();
    reDraw(0, m_bufferHead);
}

/**
 * Presses, drags and releases on the tree's right border, which drag it wider or narrower as they go.
 * @return whether the event was the border's, not the buttons' or the text's
 */
bool Editor::treeResizeMouse(const struct gfx_event& event)
{
    int x = (int16_t)event.data;

    if (event.event == GFX_EVENT_MOUSE_DOWN) {
        /* The collapsed tree's bar has no border to drag */
        if (m_treeCollapsed || x < m_treeWidth - EDITOR_TREE_GRAB_LEFT || x >= m_treeWidth + EDITOR_TREE_GRAB_RIGHT) {
            return false;
        }
        m_resizingTree = true;
        m_treeGrabX = x - m_treeWidth;
        return true;
    }

    if (!m_resizingTree) {
        return false;
    }

    /* The point that was pressed stays under the mouse */
    setTreeWidth(x - m_treeGrabX);

    if (event.event == GFX_EVENT_MOUSE_UP) {
        m_resizingTree = false;
    }
    return true;
}

/**
 * Collapses the tree to a bar at the window's left, giving the pane right of it the rest of the window, or opens it at
 * the width it had, and redraws them
 */
void Editor::setTreeCollapsed(bool collapsed)
{
    if (collapsed == m_treeCollapsed) {
        return;
    }

    m_treeCollapsed = collapsed;
    if (collapsed) {
        m_treeOpenWidth = m_treeWidth;
        m_treeWidth = EDITOR_TREE_COLLAPSED_WIDTH;
    } else {
        /* applyResolution narrows it, if the window is now too narrow for it */
        m_treeWidth = m_treeOpenWidth;
    }
    applyResolution(m_windowWidth, c_height);

    drawTreePane();
    reDrawHeader();
    reDraw(0, m_bufferHead);
}

/* Whether x, y in the window is on the tree's toggle: its button while the tree is open, or anywhere on its bar while it's collapsed */
bool Editor::treeToggleAt(int x, int y) const
{
    if (m_treeCollapsed) {
        return x >= 0 && x < m_treeWidth && y >= 0 && y < c_height;
    }

    return x >= EDITOR_TREE_TOGGLE_X && x < EDITOR_TREE_TOGGLE_X + EDITOR_TREE_TOGGLE_WIDTH &&
           y >= EDITOR_TREE_TOGGLE_Y && y < EDITOR_TREE_TOGGLE_Y + EDITOR_TREE_TOGGLE_HEIGHT;
}

/**
 * Draws the tree's toggle raised, or pushed in while it's pressed, with its arrow pointing the way a click moves the
 * tree's edge: left while it's open, and right while it's collapsed
 */
void Editor::drawTreeToggle(bool pressed)
{
    int x = EDITOR_TREE_TOGGLE_X;
    int y = EDITOR_TREE_TOGGLE_Y;
    gfx_draw_bevel(x, y, EDITOR_TREE_TOGGLE_WIDTH, EDITOR_TREE_TOGGLE_HEIGHT, pressed);

    /* On the face inside the bevel's edges, its 7-pixel column filling the face's height, a pixel down and right when pressed */
    int shift = pressed ? 1 : 0;
    int left = x + 3 + shift;
    int middle = y + ((EDITOR_TREE_TOGGLE_HEIGHT - 2) / 2) + shift;
    for (int column = 0; column < EDITOR_TREE_ARROW_COLUMNS; column++) {
        int half = m_treeCollapsed ? EDITOR_TREE_ARROW_COLUMNS - 1 - column : column;
        gfx_draw_rectangle(left + column, middle - half, 1, (half * 2) + 1, COLOR_BLACK);
    }
}

/**
 * Presses, drags and releases for the tree's toggle, as buttonMouse does for the header's buttons: a press on it draws it
 * pushed in while the mouse stays over it, and a release over it clicks it.
 * @return whether the event was the toggle's. clicked gets whether it was clicked
 */
bool Editor::treeToggleMouse(const struct gfx_event& event, bool* clicked)
{
    int x = (int16_t)event.data;
    int y = (int16_t)event.data2;
    *clicked = false;

    if (event.event == GFX_EVENT_MOUSE_DOWN) {
        m_togglePressed = treeToggleAt(x, y);
        if (!m_togglePressed) {
            return false;
        }
        m_toggleShown = true;
        drawTreeToggle(true);
        return true;
    }

    if (!m_togglePressed) {
        return false;
    }

    bool over = treeToggleAt(x, y);
    if (event.event == GFX_EVENT_MOUSE_UP) {
        *clicked = over;
        m_togglePressed = false;
        m_toggleShown = false;
        drawTreeToggle(false);
    } else if (over != m_toggleShown) {
        m_toggleShown = over;
        drawTreeToggle(over);
    }
    return true;
}

/**
 * Draws the selection's background where the text will go over it: a run of cells on each row it covers, with a selected
 * newline as a cell at its line's end. Lays out rows from view_start as reDraw does.
 */
void Editor::drawSelection(int view_start, int cols, int rows, int text_x)
{
    int start = 0;
    int end = 0;
    if (!selectionRange(start, end) || end <= view_start) {
        return;
    }

    /* The selected columns of one row, from run_from up to run_to, drawn when the row ends */
    int run_row = -1;
    int run_from = 0;
    int run_to = 0;
    auto drawRun = [&]() {
        if (run_row >= 0 && run_to > run_from) {
            gfx_draw_rectangle(text_x + (run_from * 8), TEXT_TOP + (run_row * TEXT_ROW_HEIGHT), (run_to - run_from) * 8, TEXT_ROW_HEIGHT, COLOR_SELECTED);
        }
        run_row = -1;
    };

    TextLayout at(cols);
    for (int i = view_start; i < end; i++) {
        at.place(m_textBuffer[i]);
        if (at.row >= rows) {
            break;
        }

        /* A newline after a full row has no cell left on it, so its run is empty, and drawRun leaves it out */
        if (i >= start) {
            if (run_row != at.row) {
                drawRun();
                run_row = at.row;
                run_from = at.col;
            }
            run_to = at.end;
        }
    }
    drawRun();
}

/**
 * The boundary between characters nearest x, y in the window's content, with the text laid out as reDraw draws it.
 * With under, the one before the character under x instead, for a double click. Above the text is its first row and
 * below it its last. Left of the text is a row's start, and past a row's end is its end. Past the text's last row is
 * the text's end.
 */
int Editor::indexAtPoint(int x, int y, bool under) const
{
    int cols = visibleColumns();
    int rows = visibleLines();

    int row = y < TEXT_TOP ? 0 : (y - TEXT_TOP) / TEXT_ROW_HEIGHT;
    if (row >= rows) {
        row = rows - 1;
    }

    int dx = x - textStartX();

    TextLayout at(cols);
    for (int i = viewStartIndex(cols); i < m_bufferHead; i++) {
        unsigned char ch = m_textBuffer[i];
        at.place(ch);

        /* Past the end of a row that the next character doesn't fit on is the start of the next */
        if (at.row > row) {
            return i;
        }

        /* Before the character when x is left of its middle, or with under, left of its end. A newline is its row's end */
        int edge = under ? at.end * 8 : (at.col + at.end) * 4;
        if (at.row == row && (ch == '\n' || dx < edge)) {
            return i;
        }
    }

    return m_bufferHead;
}

/* Whether c is part of a word, for a double click: a letter, a digit or _ */
static bool isWordChar(unsigned char c)
{
    return isAlpha(c) || (c >= '0' && c <= '9');
}

/**
 * Selects what's around the character at index, for a double click: its word, a run of letters, digits and _, or else
 * a run of the same character, like spaces. Past a line's end is its last character. With line, for a triple click,
 * selects its whole line and the newline after it.
 */
void Editor::selectAround(int index, bool line)
{
    int i = clampInt(index, 0, m_bufferHead);
    m_preferredColumn = -1;

    if (line) {
        int end = lineEnd(i);
        m_selAnchor = lineStart(i);
        m_bufferEdit = end < m_bufferHead ? end + 1 : end;
        return;
    }

    if ((i >= m_bufferHead || m_textBuffer[i] == '\n') && i > 0 && m_textBuffer[i - 1] != '\n') {
        i--;
    }
    if (i >= m_bufferHead || m_textBuffer[i] == '\n') {
        /* An empty line has nothing to select */
        m_selAnchor = i;
        m_bufferEdit = i;
        return;
    }

    unsigned char c = m_textBuffer[i];
    bool word = isWordChar(c);
    int start = i;
    int end = i + 1;
    while (start > 0 && (word ? isWordChar(m_textBuffer[start - 1]) : m_textBuffer[start - 1] == c)) {
        start--;
    }
    while (end < m_bufferHead && (word ? isWordChar(m_textBuffer[end]) : m_textBuffer[end] == c)) {
        end++;
    }

    m_selAnchor = start;
    m_bufferEdit = end;
}

/**
 * A press in the text puts the cursor there and starts a selection, or with Shift selects from the cursor, or the
 * selection's start, to there. Dragging moves the selection's end. Holding the mouse above or below the text scrolls it
 * (dragScrollTick).
 */
void Editor::handleMouse(const struct gfx_event& event)
{
    int x = (int16_t)event.data;
    int y = (int16_t)event.data2;
    int text_bottom = c_height - STATUS_HEIGHT;

    switch (event.event) {
    case GFX_EVENT_MOUSE_DOWN:
        if (x < m_treeWidth || y < HEADER_HEIGHT || y >= text_bottom) {
            return;
        }

        /* Typing after a click is a new undo step, even where the last one ended */
        m_undoJoin = EDITOR_UNDO_CLOSED;

        /* A double click selects the word under the mouse, and a triple its line. It's no drag, so a shaking mouse can't undo it */
        if (GFX_EVENT_CLICKS(event.flags) >= 2) {
            selectAround(indexAtPoint(x, y, true), GFX_EVENT_CLICKS(event.flags) == 3);
            m_dragging = false;
            m_dragScroll = 0;
            break;
        }

        if (event.flags & KEY_MOD_SHIFT) {
            if (m_selAnchor < 0) {
                m_selAnchor = m_bufferEdit;
            }
        } else {
            m_selAnchor = indexAtPoint(x, y);
        }
        m_bufferEdit = indexAtPoint(x, y);
        m_preferredColumn = -1;
        m_dragging = true;
        m_dragScroll = 0;
        m_dragX = x;
        break;

    case GFX_EVENT_MOUSE_MOVE:
    case GFX_EVENT_MOUSE_UP:
        if (!m_dragging) {
            return;
        }

        m_dragX = x;
        m_dragScroll = y < HEADER_HEIGHT ? -1 : (y >= text_bottom ? 1 : 0);
        m_bufferEdit = indexAtPoint(x, clampInt(y, HEADER_HEIGHT, text_bottom - 1));
        m_preferredColumn = -1;

        if (event.event == GFX_EVENT_MOUSE_UP) {
            m_dragging = false;
            m_dragScroll = 0;
        }
        break;

    default:
        return;
    }

    reDraw(0, m_bufferHead);
}

/**
 * Draws the text's scroll bar, if the text doesn't all fit, with its rows counted as the view lays them out. A view
 * scrolled past the text's end, as F3 can, counts the empty rows it shows too, so the bar can scroll back from there.
 */
void Editor::drawTextScrollBar(int cols, int rows, int view_start)
{
    int total = rowsFrom(0, cols);
    int first = total - rowsFrom(view_start, cols);
    int text_h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;

    m_textBar.place(m_treeWidth + c_width - SCROLLBAR_WIDTH, HEADER_HEIGHT, text_h);
    m_textBar.set(first + rows > total ? first + rows : total, rows, first);
    m_textBar.draw();
}

/* Scrolls the view to start at row, counting the text's rows from its start, and leaves the cursor where it is */
void Editor::scrollToRow(int row)
{
    int cols = visibleColumns();
    int index = 0;
    for (int r = 0; r < row; r++) {
        int next = nextVisualRowStart(index, cols);
        if (next <= index) {
            break;
        }
        index = next;
    }
    setScrollFromIndex(index, cols);
}

/* While a drag is held above or below the text: scrolls a row that way, if there's more text there, and selects to the row that came into view */
void Editor::dragScrollTick()
{
    if (!m_dragging || m_dragScroll == 0) {
        return;
    }

    int cols = visibleColumns();
    int rows = visibleLines();
    if (m_dragScroll < 0 || rowsFrom(viewStartIndex(cols), cols) > rows) {
        scroll(m_dragScroll);
    }

    int y = m_dragScroll < 0 ? TEXT_TOP : TEXT_TOP + ((rows - 1) * TEXT_ROW_HEIGHT);
    m_bufferEdit = indexAtPoint(m_dragX, y);
    m_preferredColumn = -1;
    reDraw(0, m_bufferHead);
}

void Editor::reDraw(int from, int to)
{
    (void)from;
    (void)to;

    int cols = visibleColumns();
    int rows = visibleLines();

    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }

    normalizeScroll(cols);

    int text_x = textStartX();
    int text_h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;
    if (text_h < 8) {
        text_h = 8;
    }

    /* From the gutter's line to the window's edge, under the header to the status line, so the padding around the text
     * is painted too. Otherwise a resize leaves it black, and a narrower gutter leaves its old digits in it (#144) */
    int pane_x = text_x - TEXT_PADDING;
    gfx_draw_rectangle(pane_x, HEADER_HEIGHT, m_treeWidth + c_width - pane_x, text_h, COLOR_BG);

    int draw_rows = rows;
    if (draw_rows > EDITOR_MAX_VIEW_ROWS) {
        draw_rows = EDITOR_MAX_VIEW_ROWS;
    }

    int row_lines[EDITOR_MAX_VIEW_ROWS];
    for (int i = 0; i < draw_rows; i++) {
        row_lines[i] = 0;
    }

    int view_start = viewStartIndex(cols);
    int line_no = lineNumberAtIndex(view_start);

    drawSelection(view_start, cols, draw_rows, text_x);

    if (draw_rows > 0) {
        row_lines[0] = line_no;
    }

    setColor(COLOR_TEXT);

    TextLayout at(cols);
    for (int i = view_start; i < m_bufferHead; i++) {
        unsigned char ch = m_textBuffer[i];
        at.place(ch);
        if (at.row >= draw_rows) {
            break;
        }

        /* The row after a newline is the next line's, even with nothing on it */
        if (ch == '\n') {
            line_no++;
            if (at.row + 1 < draw_rows) {
                row_lines[at.row + 1] = line_no;
            }
            continue;
        }
        row_lines[at.row] = line_no;

        /* A tab is blank space, with the selection's background where it's selected */
        if (ch == '\t') {
            continue;
        }

        /* A word is letters, digits and _, so a keyword at the start of a longer name, like int32_t, isn't one (#142) */
        if (isWordChar(ch)) {
            if (i == view_start || !isWordChar(m_textBuffer[i - 1])) {
                highlightWordAt(i);
            }
        } else {
            setColor(COLOR_TEXT);
        }

        if (ch < 32 || ch > 126) {
            ch = '?';
        }

        gfx_draw_char(text_x + (at.col * 8), TEXT_TOP + (at.row * TEXT_ROW_HEIGHT) + TEXT_CHAR_Y, ch, m_textColor);
    }

    drawLineNumbers(row_lines, draw_rows);

    int cursor_row = 0;
    int cursor_col = 0;
    cursorFromViewStart(view_start, cols, &cursor_row, &cursor_col);

    if (cursor_row >= 0 && cursor_row < draw_rows) {
        int cx = text_x + (cursor_col * 8);
        int cy = TEXT_TOP + (cursor_row * TEXT_ROW_HEIGHT);

        /* At the end of a full row, the cursor is past it, in the right margin, which can be narrower than a character */
        int right = m_treeWidth + c_width - SCROLLBAR_WIDTH;
        if (cx > text_x + (cols * 8) - 8 && cx < right) {
            gfx_draw_rectangle(cx, cy, minInt(8, right - cx), TEXT_ROW_HEIGHT, COLOR_VGA_LIGHT_GRAY);
        }

        if (cx <= text_x + (cols * 8) - 8) {
            gfx_draw_rectangle(cx, cy, 8, TEXT_ROW_HEIGHT, COLOR_VGA_LIGHT_GRAY);

            if (m_bufferEdit < m_bufferHead && m_textBuffer[m_bufferEdit] != '\n') {
                unsigned char ch = m_textBuffer[m_bufferEdit];
                if (ch >= 32 && ch <= 126) {
                    if (isWordChar(ch)) {
                        highlightWordAt(m_bufferEdit);
                    } else {
                        setColor(COLOR_TEXT);
                    }
                    gfx_draw_char(cx, cy + TEXT_CHAR_Y, ch, m_textColor);
                }
            }
        }
    }

    drawTextScrollBar(cols, rows, view_start);
    drawStatus();
}

/* The status line: the prompt, while one is open, or the file's name and the cursor's position, unless a message is kept there */
void Editor::drawStatus()
{
    if (m_prompt != EDITOR_PROMPT_NONE) {
        drawPrompt();
        return;
    }

    int line = 1;
    int colnum = 1;
    getCursorLineCol(line, colnum);

    const char* file_label = m_currentPath[0] ? m_currentPath : "(untitled)";
    const char* dirty_tag = m_readOnly ? " (read-only)" : hasUnsavedChanges() ? "*" : "";

    int sel_start = 0;
    int sel_end = 0;
    bool selected = selectionRange(sel_start, sel_end);

    /**
     * A long path keeps its end, after "...", so the line and column, and how much is selected, stay in view. Room is
     * kept for a "*" whether or not there is one, so the path doesn't move when there is
     */
    const char* dots = "";
    int tag_len = m_readOnly ? (int)strlen(dirty_tag) : 1;
    int room = textClipWidth() / 8 - tag_len - (int)strlen(selected ? "  Ln:99999 Col:9999  65535 selected" : "  Ln:99999 Col:9999");
    int label_len = strlen(file_label);
    if (room > 4 && label_len > room) {
        file_label += label_len - (room - 3);
        dots = "...";
    }

    /* A message kept on the status line stays until the next event, a key, a click or a resize (EditorLoop) */
    if (m_keepStatus) {
        return;
    }
    if (selected) {
        drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "%s%s%s  Ln:%d Col:%d  %d selected", dots, file_label, dirty_tag, line, colnum, sel_end - sel_start);
        return;
    }
    drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "%s%s%s  Ln:%d Col:%d", dots, file_label, dirty_tag, line, colnum);
}

void Editor::setColor(color_t color)
{
    m_textColor = color;
}

void Editor::highlightSyntax(unsigned char* start)
{
    if (start == nullptr) {
        setColor(COLOR_TEXT);
        return;
    }

    int tokenLen = 0;
    while (isWordChar(start[tokenLen])) {
        tokenLen++;
    }

    if (tokenLen == 0) {
        setColor(COLOR_TEXT);
        return;
    }

    for (int i = 0; i < 20; i++) {
        struct keyword* key = &keyWords[i];
        if (key->color == 0) {
            break;
        }

        int keyLen = strlen(key->word);
        if (tokenLen == keyLen && memcmp(start, key->word, keyLen) == 0) {
            setColor(key->color);
            return;
        }
    }

    setColor(COLOR_TEXT);
}

/* Sets the color of the word index is in, from the word's start */
void Editor::highlightWordAt(int index)
{
    int start = index;
    while (start > 0 && isWordChar(m_textBuffer[start - 1])) {
        start--;
    }
    highlightSyntax(&m_textBuffer[start]);
}

int Editor::countLines() const
{
    int lines = 1;
    for (int i = 0; i < m_bufferHead; i++) {
        if (m_textBuffer[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

/* Whether the text differs from the last save, which undoing or redoing back to it makes it not. Without a history, whether it was edited */
bool Editor::hasUnsavedChanges() const
{
    if (m_undo != nullptr) {
        return m_undoDone != m_undoSaved;
    }
    return m_dirty != 0;
}

/* The cursor's line and column, from 1, for the status line. A tab ends at a tab stop, as in a desktop editor */
void Editor::getCursorLineCol(int& line, int& col) const
{
    line = lineNumberAtIndex(m_bufferEdit);
    col = lineColumn(m_bufferEdit) + 1;
}

void Editor::Lex()
{
}

/* Forgets what's kept about the text's past: the } that typing { added, and the undo history. The text is the caller's */
void Editor::clearHistory()
{
    m_autoBraceCount = 0;
    undoClear();
}

/* len characters went in at index: the } at or after it move along */
void Editor::autoBracesInserted(int index, int len)
{
    for (int i = 0; i < m_autoBraceCount; i++) {
        if (m_autoBraces[i] >= index) {
            m_autoBraces[i] += len;
        }
    }
}

/* count characters went from index: the } among them are forgotten, and those after them move back */
void Editor::autoBracesDeleted(int index, int count)
{
    int kept = 0;
    for (int i = 0; i < m_autoBraceCount; i++) {
        int at = m_autoBraces[i];
        if (at >= index && at < index + count) {
            continue;
        }
        m_autoBraces[kept++] = at >= index + count ? at - count : at;
    }
    m_autoBraceCount = kept;
}

/* @return which of m_autoBraces is the } at index, or -1 if that's not one typing { added */
int Editor::autoBraceAt(int index) const
{
    for (int i = m_autoBraceCount - 1; i >= 0; i--) {
        if (m_autoBraces[i] == index) {
            return i;
        }
    }
    return -1;
}

/**
 * The four functions that change the text, m_textBuffer's first m_bufferHead characters, with a 0 after them. Each
 * moves the } kept in m_autoBraces, and records the change for undo (#161). The text holds at most m_bufferSize - 1
 * characters.
 */

/* Inserts c at index. @return false without the room */
bool Editor::bufferInsertChar(int index, unsigned char c)
{
    return bufferInsertText(index, &c, 1);
}

/* Inserts len characters at index, moving the rest along. @return false, having changed nothing, without the room */
bool Editor::bufferInsertText(int index, const unsigned char* data, int len)
{
    if (len <= 0) {
        return true;
    }
    if (index < 0 || index > m_bufferHead || m_bufferHead + len > m_bufferSize - 1) {
        return false;
    }

    memmove(&m_textBuffer[index + len], &m_textBuffer[index], m_bufferHead - index);
    memcpy(&m_textBuffer[index], data, len);
    m_bufferHead += len;
    m_textBuffer[m_bufferHead] = 0;
    autoBracesInserted(index, len);
    undoInserted(index, data, len);
    return true;
}

/* Deletes the character at index. @return false if there's none */
bool Editor::bufferDeleteChar(int index)
{
    return index >= 0 && index < m_bufferHead && bufferDeleteRange(index, 1);
}

/* Deletes count characters from index, moving the rest back. @return false, having changed nothing, if they aren't all in the text */
bool Editor::bufferDeleteRange(int index, int count)
{
    if (index < 0 || count < 0 || index + count > m_bufferHead) {
        return false;
    }
    if (count == 0) {
        return true;
    }

    /* Recorded while they're still there */
    undoDeleting(index, count);
    memmove(&m_textBuffer[index], &m_textBuffer[index + count], m_bufferHead - index - count);
    m_bufferHead -= count;
    m_textBuffer[m_bufferHead] = 0;
    autoBracesDeleted(index, count);
    return true;
}

/* Whether some text is selected: the anchor is set, and the cursor has moved from it */
bool Editor::hasSelection() const
{
    return m_selAnchor >= 0 && m_selAnchor != m_bufferEdit;
}

/* The selected text, from start up to end, in order whichever way it was selected. @return false if there's none */
bool Editor::selectionRange(int& start, int& end) const
{
    if (!hasSelection()) {
        return false;
    }

    start = minInt(m_selAnchor, m_bufferEdit);
    end = m_selAnchor > m_bufferEdit ? m_selAnchor : m_bufferEdit;
    return true;
}

void Editor::clearSelection()
{
    m_selAnchor = -1;
}

/* Deletes the text from start up to end, and puts the cursor where it was. @return false if there was nothing to delete */
bool Editor::deleteRange(int start, int end)
{
    start = clampInt(start, 0, m_bufferHead);
    end = clampInt(end, start, m_bufferHead);
    if (end == start || !bufferDeleteRange(start, end - start)) {
        return false;
    }

    m_bufferEdit = start;
    m_dirty = 1;
    m_preferredColumn = -1;
    return true;
}

/* Deletes the selected text, leaving the cursor where it started. @return false if nothing was selected */
bool Editor::deleteSelection()
{
    int start = 0;
    int end = 0;
    if (!selectionRange(start, end)) {
        return false;
    }

    clearSelection();
    return deleteRange(start, end);
}

/* Inserts len characters at index, and puts the cursor after them. @return false without the room or the memory */
bool Editor::insertText(int index, const unsigned char* data, int len)
{
    if (!bufferInsertText(index, data, len)) {
        return false;
    }

    m_bufferEdit = index + len;
    m_dirty = 1;
    m_preferredColumn = -1;
    return true;
}

/* Copies the selected text to the clipboard. @return false if nothing was selected, or it couldn't be copied, which the status line says */
bool Editor::copySelection()
{
    int start = 0;
    int end = 0;
    if (!selectionRange(start, end)) {
        return false;
    }

    int ret = clipboard_set((const char*)&m_textBuffer[start], end - start);
    if (ret < 0) {
        drawStatusLine(COLOR_VGA_RED, "Can't copy: %s", error_get_string(ret));
        return false;
    }
    return true;
}

/**
 * Puts the clipboard's text in place of the selection, or at the cursor, exactly as it is: without the } that typing
 * a { adds (#119). What doesn't fit in the buffer is left out, and the status line says so.
 */
void Editor::paste()
{
    int size = clipboard_get(nullptr, 0);
    if (size <= 0) {
        return;
    }

    int start = m_bufferEdit;
    int end = m_bufferEdit;
    selectionRange(start, end);
    int room = (m_bufferSize - 1) - (m_bufferHead - (end - start));
    int len = minInt(size, room);
    if (len <= 0) {
        drawStatusLine(COLOR_VGA_RED, "Can't paste: the Editor holds %d characters.", m_bufferSize - 1);
        return;
    }

    unsigned char* data = (unsigned char*) malloc(len);
    if (data == nullptr) {
        drawStatusLine(COLOR_VGA_RED, "Can't paste: out of memory.");
        return;
    }

    /* Another program may have changed the clipboard since it was measured */
    size = clipboard_get((char*)data, len);
    if (size < len) {
        len = size > 0 ? size : 0;
    }

    /* An anchor with nothing selected yet, as after a click, would select the pasted text */
    deleteSelection();
    clearSelection();
    bool pasted = insertText(m_bufferEdit, data, len);
    free(data);

    if (!pasted) {
        drawStatusLine(COLOR_VGA_RED, "Can't paste: out of memory.");
    } else if (len < size) {
        drawStatusLine(COLOR_VGA_RED, "Pasted %d of %d characters: the Editor holds %d.", len, size, m_bufferSize - 1);
    }
}

/* The bytes a step with text characters, deleted and inserted, takes in the history */
int Editor::undoStepBytes(int text)
{
    return (int)sizeof(struct undo_step) + ((text + 3) & ~3);
}

/* Empties the undo history and frees it, as the text is replaced. Unsaved changes count from here */
void Editor::undoClear()
{
    if (m_undo != nullptr) {
        free(m_undo);
        m_undo = nullptr;
    }

    m_undoSize = 0;
    m_undoUsed = 0;
    m_undoAt = 0;
    m_undoLast = -1;
    m_undoDone = 0;
    m_undoDropped = 0;
    m_undoSaved = 0;
    m_undoJoin = EDITOR_UNDO_CLOSED;
    m_undoFailed = false;
}

/* Out of memory for the history: edits go on unrecorded until the text is replaced, and m_dirty says they were made */
void Editor::undoFail()
{
    undoClear();
    m_undoFailed = true;
    m_dirty = 1;
}

/**
 * Makes room for need more bytes after the steps. The history grows, twice the size each time up to EDITOR_UNDO_MAX,
 * or else drops its oldest steps, but not the one at keep or any after it.
 * @return false without the memory, having turned the history off
 */
bool Editor::undoReserve(int need, int keep)
{
    if (m_undoUsed + need <= m_undoSize) {
        return true;
    }

    int size = m_undoSize > 0 ? m_undoSize : EDITOR_UNDO_MIN;
    while (size < m_undoUsed + need && size < EDITOR_UNDO_MAX) {
        size = minInt(size * 2, EDITOR_UNDO_MAX);
    }
    if (size > m_undoSize) {
        unsigned char* grown = (unsigned char*) malloc(size);
        if (grown != nullptr) {
            if (m_undo != nullptr) {
                memcpy(grown, m_undo, m_undoUsed);
                free(m_undo);
            }
            m_undo = grown;
            m_undoSize = size;
        }
    }

    int drop = 0;
    int steps = 0;
    while (m_undoUsed - drop + need > m_undoSize && drop < keep) {
        struct undo_step* oldest = (struct undo_step*)&m_undo[drop];
        drop += undoStepBytes(oldest->del + oldest->ins);
        steps++;
    }
    if (m_undoUsed - drop + need > m_undoSize) {
        undoFail();
        return false;
    }

    if (drop > 0) {
        memmove(m_undo, &m_undo[drop], m_undoUsed - drop);
        m_undoUsed -= drop;
        m_undoAt -= drop;
        m_undoLast = m_undoLast >= drop ? m_undoLast - drop : -1;
        m_undoDropped += steps;
        /* The last save was before the oldest step left, so undo can't get back to it */
        if (m_undoSaved < m_undoDropped) {
            m_undoSaved = -1;
        }
        if (m_undoUsed > 0) {
            ((struct undo_step*)m_undo)->prev = 0;
        }
    }
    return true;
}

/**
 * The step to record an edit of len characters at index in, with room after its text for them, which the caller copies
 * in: the last step if it's open and the edit joins its text, or else a new one.
 * @return nullptr if the edit isn't recorded: it's undo's or redo's own, or the history is off or just ran out of memory
 */
Editor::undo_step* Editor::undoStepFor(int index, int len, bool inserted)
{
    if (len <= 0 || m_undoing || m_undoFailed) {
        return nullptr;
    }

    /* A new edit after undoing: the steps undone can't be redone after it */
    if (m_undoAt < m_undoUsed) {
        if (m_undoSaved > m_undoDone) {
            m_undoSaved = -1;
        }
        m_undoUsed = m_undoAt;
    }

    /**
     * An insert joins anywhere in the step's inserted text, since typing { leaves the cursor before the } it adds. A
     * delete joins a step that only deletes, at either end of its text, as Backspace and Delete go
     */
    struct undo_step* step = nullptr;
    if (m_undoJoin != EDITOR_UNDO_CLOSED && m_undoLast >= 0) {
        step = (struct undo_step*)&m_undo[m_undoLast];
    }
    bool joins = step != nullptr && (inserted ? index >= step->pos && index <= step->pos + step->ins
                                              : step->ins == 0 && index <= step->pos && step->pos <= index + len);
    int text = joins ? step->del + step->ins : 0;
    if (!undoReserve(undoStepBytes(text + len) - (joins ? undoStepBytes(text) : 0), joins ? m_undoLast : m_undoUsed)) {
        return nullptr;
    }

    if (joins) {
        step = (struct undo_step*)&m_undo[m_undoLast];
    } else {
        step = (struct undo_step*)&m_undo[m_undoUsed];
        step->prev = m_undoLast >= 0 ? m_undoUsed - m_undoLast : 0;
        step->pos = index;
        step->del = 0;
        step->ins = 0;
        step->cursorBefore = m_undoCursor;
        step->anchorBefore = m_undoAnchor;
        m_undoLast = m_undoUsed;
        m_undoDone++;
        m_undoJoin = m_undoKey != EDITOR_UNDO_CLOSED ? m_undoKey : EDITOR_UNDO_ALONE;
    }

    /* Until the key ends, and says where it left the cursor */
    step->cursorAfter = inserted ? index + len : index;
    m_undoUsed = m_undoLast + undoStepBytes(text + len);
    m_undoAt = m_undoUsed;
    return step;
}

/* Records for undo that len characters went in at index, from data */
void Editor::undoInserted(int index, const unsigned char* data, int len)
{
    struct undo_step* step = undoStepFor(index, len, true);
    if (step == nullptr) {
        return;
    }

    unsigned char* ins = (unsigned char*)(step + 1) + step->del;
    int at = index - step->pos;
    memmove(&ins[at + len], &ins[at], step->ins - at);
    memcpy(&ins[at], data, len);
    step->ins += len;
}

/* Records for undo that len characters are about to go from index, copying them out of the text while they're there */
void Editor::undoDeleting(int index, int len)
{
    struct undo_step* step = undoStepFor(index, len, false);
    if (step == nullptr) {
        return;
    }

    /* Those before the step's position go in front of its text, as Backspace deletes, and the rest after, as Delete does */
    unsigned char* del = (unsigned char*)(step + 1);
    int ahead = step->pos - index;
    memmove(&del[ahead], del, step->del);
    if (ahead > 0) {
        memcpy(del, &m_textBuffer[index], ahead);
    }
    if (len - ahead > 0) {
        memcpy(&del[ahead + step->del], &m_textBuffer[step->pos], len - ahead);
    }
    step->del += len;
    step->pos = index;
}

/**
 * Before a key: its edits join the last step if both are typing, or both Backspace or both Delete, carrying on from
 * where the last left the cursor. Otherwise they make a new step, which starts from the cursor and selection now.
 */
void Editor::undoKeyStart(int kind)
{
    bool joinable = kind == EDITOR_UNDO_TYPING || kind == EDITOR_UNDO_BACKSPACE || kind == EDITOR_UNDO_DELETE;
    if (!joinable || kind != m_undoJoin || m_undoLast < 0 || m_undoAt != m_undoUsed || hasSelection() ||
        m_bufferEdit != ((struct undo_step*)&m_undo[m_undoLast])->cursorAfter) {
        m_undoJoin = EDITOR_UNDO_CLOSED;
    }

    m_undoKey = kind;
    m_undoCursor = m_bufferEdit;
    m_undoAnchor = hasSelection() ? m_selAnchor : -1;
}

/* After a key: the step it made or joined ends with the cursor where the key left it. Enter, Tab, cut and paste end it */
void Editor::undoKeyEnd()
{
    m_undoKey = EDITOR_UNDO_CLOSED;
    if (m_undoJoin == EDITOR_UNDO_CLOSED || m_undoLast < 0) {
        return;
    }

    ((struct undo_step*)&m_undo[m_undoLast])->cursorAfter = m_bufferEdit;
    if (m_undoJoin == EDITOR_UNDO_ALONE) {
        m_undoJoin = EDITOR_UNDO_CLOSED;
    }
}

/**
 * Changes the text as the step did, or back as it was: the text going out comes out, then the text coming in goes in.
 * The result is the text from before or after the step, which fitted then, but that's checked first, so a failure
 * changes nothing.
 * @return false if the result wouldn't fit
 */
bool Editor::undoApply(const struct undo_step* step, bool redo)
{
    const unsigned char* del = (const unsigned char*)(step + 1);
    const unsigned char* ins = del + step->del;
    int out = redo ? step->del : step->ins;
    const unsigned char* in = redo ? ins : del;
    int in_len = redo ? step->ins : step->del;
    if (step->pos + out > m_bufferHead || m_bufferHead - out + in_len > m_bufferSize - 1) {
        return false;
    }

    m_undoing = true;
    bool done = bufferDeleteRange(step->pos, out) && bufferInsertText(step->pos, in, in_len);
    m_undoing = false;
    return done;
}

/* Ctrl+Z: takes the last step back, and puts back the cursor and selection from before it */
void Editor::undo()
{
    m_undoJoin = EDITOR_UNDO_CLOSED;
    if (m_undoFailed) {
        drawStatusLine(COLOR_VGA_RED, "Can't undo: out of memory.");
        return;
    }
    if (m_undoLast < 0) {
        drawStatusLine(COLOR_VGA_RED, "Nothing to undo.");
        return;
    }

    struct undo_step* step = (struct undo_step*)&m_undo[m_undoLast];
    if (!undoApply(step, false)) {
        drawStatusLine(COLOR_VGA_RED, "Can't undo: out of memory.");
        return;
    }

    m_undoAt = m_undoLast;
    m_undoLast = step->prev > 0 ? m_undoLast - step->prev : -1;
    m_undoDone--;
    m_bufferEdit = clampInt(step->cursorBefore, 0, m_bufferHead);
    m_selAnchor = step->anchorBefore < 0 ? -1 : clampInt(step->anchorBefore, 0, m_bufferHead);
    m_preferredColumn = -1;
}

/* Ctrl+Y or Ctrl+Shift+Z: makes the last step undone again, and puts the cursor after it */
void Editor::redo()
{
    m_undoJoin = EDITOR_UNDO_CLOSED;
    if (m_undoAt >= m_undoUsed) {
        drawStatusLine(COLOR_VGA_RED, "Nothing to redo.");
        return;
    }

    struct undo_step* step = (struct undo_step*)&m_undo[m_undoAt];
    if (!undoApply(step, true)) {
        drawStatusLine(COLOR_VGA_RED, "Can't redo: out of memory.");
        return;
    }

    m_undoLast = m_undoAt;
    m_undoAt += undoStepBytes(step->del + step->ins);
    m_undoDone++;
    m_bufferEdit = clampInt(step->cursorAfter, 0, m_bufferHead);
    clearSelection();
    m_preferredColumn = -1;
}

void Editor::scroll(int lines)
{
    if (lines == 0) {
        return;
    }

    int cols = visibleColumns();
    normalizeScroll(cols);

    if (lines > 0) {
        while (lines > 0) {
            int start = viewStartIndex(cols);
            int next = nextVisualRowStart(start, cols);
            if (next == start) {
                break;
            }
            setScrollFromIndex(next, cols);
            lines--;
        }
    } else {
        while (lines < 0) {
            int start = viewStartIndex(cols);
            int prev = prevVisualRowStart(start, cols);
            if (prev == start) {
                break;
            }
            setScrollFromIndex(prev, cols);
            lines++;
        }
    }

    normalizeScroll(cols);
}

void Editor::Reset()
{
    clearHistory();

    memset(m_textBuffer, 0, m_bufferSize);
    m_bufferHead = 0;
    m_bufferEdit = 0;
    m_fileSize = 0;
    m_dirty = 0;
    m_readOnly = false;
    m_preferredColumn = -1;
    m_selAnchor = -1;
    scrollY = 0;
    m_scrollWrap = 0;
    m_currentPath[0] = 0;

    /* Without a path, no file in the tree is highlighted */
    drawTreePane();
    reDrawHeader();
    reDraw(0, 0);
}

bool Editor::SaveMsg()
{
    if (!hasUnsavedChanges()) {
        return true;
    }

    MsgBox msg("Save changes?", "Save changes to file?", MSGBOX_YES_NO_CANCEL);
    MsgBoxResult ret = msg.show();

    if (ret == MSGBOX_YES) {
        return Save();
    }

    if (ret == MSGBOX_NO) {
        return true;
    }

    return false;
}

bool Editor::Quit()
{
    if (!SaveMsg()) {
        reDraw(0, m_bufferHead);
        return false;
    }

    if (m_fd >= 0) {
        fclose(m_fd);
        m_fd = -1;
    }

    exit();
    return true;
}

bool Editor::Open(char* path)
{
    if (path == nullptr || path[0] == 0) {
        return false;
    }

    /**
     * Copied before the save question: saving a new file reloads the tree, which frees the name a click on it passed
     * as path
     */
    static char absolute[FS_PATH_MAX];
    if (editor_absolute_path(path, absolute) < 0) {
        drawStatusLine(COLOR_VGA_RED, "Can't open %s: the path is too long.", path);
        return false;
    }

    /* Unsaved changes count whether or not the file is on disk yet. Saving reopens m_fd, so it's closed after this */
    if (!SaveMsg()) {
        reDraw(0, m_bufferHead);
        return false;
    }

    /**
     * A name that isn't there is a new file, which the first save creates. Any other error, like a missing directory,
     * is reported now rather than at the save
     */
    int new_fd = open_editor_file(absolute);
    bool is_new = new_fd == -ERROR_FILE_NOT_FOUND;
    if (new_fd < 0 && !is_new) {
        drawStatusLine(COLOR_VGA_RED, "Can't open %s: %s", absolute, error_get_string(new_fd));
        return false;
    }

    if (m_fd >= 0) {
        fclose(m_fd);
    }
    m_fd = is_new ? -1 : new_fd;

    char title[EDITOR_TITLE_MAX + 1];
    editor_title(absolute, title, sizeof(title));
    setTitle(title);

    strncpy(m_currentPath, absolute, (uint32_t)(sizeof(m_currentPath) - 1));
    m_currentPath[sizeof(m_currentPath) - 1] = 0;

    memset(m_textBuffer, 0, m_bufferSize);

    /* As much as fits. A failed read loads nothing, and leaves no path, so nothing can be saved over the file */
    int read_bytes = 0;
    int read_error = 0;
    while (m_fd >= 0 && read_bytes < m_bufferSize - 1) {
        int ret = read(m_fd, m_textBuffer + read_bytes, m_bufferSize - 1 - read_bytes);
        if (ret < 0) {
            read_error = ret;
        }
        if (ret <= 0) {
            break;
        }
        read_bytes += ret;
    }

    /* A file bigger than the buffer opens read-only, since saving what fits would cut it short (#133) */
    m_readOnly = false;
    if (read_error == 0 && m_fd >= 0 && read_bytes == m_bufferSize - 1) {
        unsigned char more;
        m_readOnly = read(m_fd, &more, 1) != 0;
    }

    if (read_error < 0) {
        read_bytes = 0;
    }

    m_textBuffer[read_bytes] = 0;
    m_bufferHead = read_bytes;

    if (read_error < 0) {
        if (m_fd >= 0) {
            fclose(m_fd);
            m_fd = -1;
        }
        /* An empty, untitled text, with nothing kept from the last file */
        setTitle("Editor");
        Reset();
        drawStatusLine(COLOR_VGA_RED, "Can't read %s: %s", absolute, error_get_string(read_error));
        return false;
    }

    /* A new text, so nothing from the last one's past applies */
    clearHistory();

    m_bufferEdit = 0;
    scrollY = 0;
    m_scrollWrap = 0;
    m_fileSize = m_bufferHead;
    m_dirty = 0;
    m_preferredColumn = -1;
    m_selAnchor = -1;

    /* However it was opened, the file's row in the tree shows, highlighted, if the tree has it */
    if (treeView != nullptr) {
        treeView->reveal(m_currentPath);
    }
    drawTreePane();
    reDrawHeader();
    reDraw(0, m_bufferHead);
    if (is_new) {
        drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "New file: Ctrl+S saves it to disk.");
        m_keepStatus = 1;
    }
    if (m_readOnly) {
        static struct fs_dirent info;
        if (stat(m_currentPath, &info) >= 0) {
            drawStatusLine(COLOR_VGA_RED, "Read-only: showing %d of %d bytes.", m_bufferHead, (int)info.size);
        } else {
            drawStatusLine(COLOR_VGA_RED, "Read-only: showing the first %d bytes.", m_bufferHead);
        }
    }
    return true;
}

void Editor::setFd(int fd)
{
    m_fd = fd;
}

/**
 * Saves an untitled text to a new file. It asks for the path until it's given one where nothing is yet, so it never
 * replaces a file, or until it's canceled. @return whether it saved
 */
bool Editor::SaveAs()
{
    /* Static, to keep them off the app's stack. What was typed is in the box again after an error, to change */
    static char typed[FS_PATH_MAX];
    static char absolute[FS_PATH_MAX];
    static struct fs_dirent existing;
    typed[0] = 0;

    while (1) {
        if (!showNewFileDialog(typed, sizeof(typed))) {
            return false;
        }

        const char* problem = nullptr;
        int ret = 0;
        if (editor_absolute_path(typed, absolute) < 0) {
            problem = "Path too long.";
        } else if ((ret = stat(absolute, &existing)) >= 0) {
            problem = (existing.attributes & FS_ATTR_DIRECTORY) ? error_get_string(-ERROR_IS_DIRECTORY) : "File Already Exists";
        } else if (ret != -ERROR_FILE_NOT_FOUND) {
            /* Like a directory in the path that isn't there */
            problem = error_get_string(ret);
        }

        if (problem == nullptr) {
            break;
        }

        MsgBox msg("Save As", problem, MSGBOX_OK);
        msg.show();
    }

    strncpy(m_currentPath, absolute, (uint32_t)(sizeof(m_currentPath) - 1));
    m_currentPath[sizeof(m_currentPath) - 1] = 0;

    bool saved = Save();
    if (m_fd < 0) {
        /* No file was made, so the text stays untitled, and the next save asks again. Save said why */
        m_currentPath[0] = 0;
        return false;
    }

    char title[EDITOR_TITLE_MAX + 1];
    editor_title(m_currentPath, title, sizeof(title));
    setTitle(title);
    return saved;
}

bool Editor::Save()
{
    if (m_fd < 0 && m_currentPath[0] == 0) {
        return SaveAs();
    }

    if (m_readOnly) {
        drawStatusLine(COLOR_VGA_RED, "Read-only: too big for the Editor to save.");
        return false;
    }

    /* A file this save creates is added to the tree. Static, to keep it off the app's stack */
    static struct fs_dirent existing;
    bool creates = m_currentPath[0] != 0 && stat(m_currentPath, &existing) == -ERROR_FILE_NOT_FOUND;

    if (m_currentPath[0] != 0) {
        if (m_fd >= 0) {
            fclose(m_fd);
            m_fd = -1;
        }

        int fd = open_editor_file_for_save(m_currentPath);
        if (fd < 0) {
            drawStatusLine(COLOR_VGA_RED, "Can't save %s: %s", m_currentPath, error_get_string(fd));
            return false;
        }
        m_fd = fd;
    }

    int ret = write(m_fd, m_textBuffer, m_bufferHead);
    if (ret < 0) {
        drawStatusLine(COLOR_VGA_RED, "Can't save %s: %s", m_currentPath, error_get_string(ret));
        return false;
    }
    if (ret != m_bufferHead) {
        /* A short write means the disk is full */
        drawStatusLine(COLOR_VGA_RED, "Saved only %d of %d bytes: %s", ret, m_bufferHead, error_get_string(-ERROR_NO_SPACE));
        return false;
    }

    m_fileSize = m_bufferHead;
    m_dirty = 0;
    /* Typing on after a save starts a new step, so undo can get back to what was saved */
    m_undoSaved = m_undoDone;
    m_undoJoin = EDITOR_UNDO_CLOSED;
    if (creates && treeView != nullptr) {
        /* Reloading collapses every directory, so the new file's are expanded again */
        treeView->refresh();
        treeView->reveal(m_currentPath);
        drawTreePane();
    }
    reDraw(0, m_bufferHead);
    return true;
}

void Editor::applyResolution(int window_width, int window_height)
{
    /* The tree keeps its width, unless the window is now too narrow for it. Its collapsed bar always fits */
    m_windowWidth = window_width;
    if (!m_treeCollapsed) {
        m_treeWidth = clampTreeWidth(m_treeWidth);
    }

    c_width = window_width - m_treeWidth;
    if (c_width < EDITOR_MIN_RIGHT_PANE_WIDTH) {
        c_width = EDITOR_MIN_RIGHT_PANE_WIDTH;
    }

    c_height = window_height;
    if (c_height < EDITOR_MIN_HEIGHT) {
        c_height = EDITOR_MIN_HEIGHT;
    }

    /* Collapsed, the tree keeps its size, and gets the window's when it's opened */
    if (treeView != nullptr && !m_treeCollapsed) {
        treeView->resize(m_treeWidth, c_height);
    }

    int cols = visibleColumns();
    int rows = visibleLines();
    ensureCursorVisible(cols, rows);
}

/**
 * Asks for the path to save a new file to, in a window of its own. path holds what the box starts with, and gets what
 * was typed. @return false if it was canceled, or the window couldn't be shown, which the status line says
 */
bool Editor::showNewFileDialog(char* path, int path_size)
{
    if (path == nullptr || path_size <= 1) {
        return false;
    }

    /* Must live on heap: user threads have separate stacks in this kernel. */
    EditorNewFilePopupShared* shared = (EditorNewFilePopupShared*) malloc(sizeof(EditorNewFilePopupShared));
    if (shared == nullptr) {
        drawStatusLine(COLOR_VGA_RED, "Save As: out of memory.");
        return false;
    }

    memset(shared, 0, sizeof(EditorNewFilePopupShared));
    shared->running = 1;
    shared->result = EDITOR_NEW_FILE_POPUP_RESULT_CANCEL;
    strncpy(shared->path, path, (uint32_t)(sizeof(shared->path) - 1));

    Thread popup_thread(__editor_new_file_popup_thread, 0);
    int thread_id = popup_thread.start((void*)shared);
    if (thread_id < 0) {
        drawStatusLine(COLOR_VGA_RED, "Failed to open Save As window.");
        free(shared);
        return false;
    }

    int await_ret = invoke_syscall(SYSCALL_AWAIT_PROCESS, thread_id, 0, 0);
    if (await_ret < 0) {
        drawStatusLine(COLOR_VGA_RED, "Failed waiting for Save As window.");
        free(shared);
        return false;
    }

    if (shared->result == EDITOR_NEW_FILE_POPUP_RESULT_ERROR) {
        drawStatusLine(COLOR_VGA_RED, "Save As: out of memory.");
        free(shared);
        return false;
    }

    if (shared->result != EDITOR_NEW_FILE_POPUP_RESULT_CREATE || shared->path[0] == 0) {
        drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "Not saved.");
        free(shared);
        return false;
    }

    strncpy(path, shared->path, (uint32_t)(path_size - 1));
    path[path_size - 1] = 0;
    free(shared);
    return true;
}

/* Starts an empty, untitled text, which the first save names. Unsaved changes are asked about first */
void Editor::New()
{
    if (!SaveMsg()) {
        return;
    }

    if (m_fd >= 0) {
        fclose(m_fd);
        m_fd = -1;
    }
    setTitle("Editor");
    Reset();
}

/* The Help window's rows, with "" for an empty row */
static const char* const editor_help[] = {
    "Ctrl+S: Save file",
    "Ctrl+N: New file",
    "Ctrl+H: Show this help",
    "Ctrl+B: Hide/show the file tree",
    "F2/F3: Scroll up/down",
    "",
    "Arrow keys: Move cursor",
    "Home/End: Start/end of line",
    "PgUp/PgDn: Move a page",
    "Ctrl+G: Go to line",
    "Ctrl+F: Find text",
    "  Enter/Shift+Enter: Next/prev",
    "Shift+move: Select",
    "Ctrl+A: Select all",
    "Esc: Clear selection",
    "Ctrl+C/X/V: Copy/cut/paste",
    "Ctrl+Z/Y: Undo/redo",
    "Backspace: Delete character",
    "Delete: Delete next character",
    "Enter: New line",
    "Tab: Insert spaces",
    "",
    "Mouse: Click file in tree to open",
    "Drag over text: Select",
    "Shift+click: Select to there",
    "Double/triple click: Word/line",
    "",
    "Esc closes this window.",
};

#define EDITOR_HELP_ROWS ((int)(sizeof(editor_help) / sizeof(editor_help[0])))
/* Wide enough for the longest row, and tall enough for them all, as the taskbar's Readme lays them out */
#define EDITOR_HELP_WIDTH 300
#define EDITOR_HELP_HEIGHT (24 + (EDITOR_HELP_ROWS * 10))

/* How long the Help window waits for an event before it looks whether the Editor wants it closed */
#define EDITOR_HELP_POLL_MS 50

/**
 * The Help window's state, which its thread and the Editor share as the program's globals. editor_help_open is set by
 * the Editor as it starts the thread, whose pid is editor_help_pid, and cleared by the thread as it closes. The Editor
 * sets editor_help_close to have it close
 */
static volatile int editor_help_open = 0;
static volatile int editor_help_pid = -1;
static volatile int editor_help_close = 0;

/**
 * The Help window, drawn as the taskbar's Readme is (display_note in kernel/kthreads/about.c): the rows in a box, which
 * Up and Down scroll when they don't all fit. Esc or its close button closes it. It runs on a thread of its own, which
 * the Editor doesn't wait for, so the text can be edited while it's open, and it closes with the Editor.
 */
static void __editor_help_thread(void* arg)
{
    (void)arg;

    Window help(EDITOR_HELP_WIDTH, EDITOR_HELP_HEIGHT, "Editor Help", 0);

    int shown = (EDITOR_HELP_HEIGHT - 24) / 10;
    int first = 0;
    bool redraw = true;

    while (!editor_help_close) {
        if (redraw) {
            help.drawRect(0, 0, EDITOR_HELP_WIDTH, EDITOR_HELP_HEIGHT, COLOR_BAR_FACE);
            help.drawContouredBox(10, 10, EDITOR_HELP_WIDTH - 20, EDITOR_HELP_HEIGHT - 20, COLOR_BAR_LIGHT);
            for (int row = 0; row < shown && first + row < EDITOR_HELP_ROWS; row++) {
                help.drawText(12, 12 + (row * 10), editor_help[first + row], COLOR_BLACK);
            }
            redraw = false;
        }

        /* A little at a time, so a close the Editor asks for is seen within EDITOR_HELP_POLL_MS */
        struct gfx_event event;
        if (gfx_get_event(&event, GFX_EVENT_NONBLOCKING) < 0) {
            sleep(EDITOR_HELP_POLL_MS);
            continue;
        }

        if (event.event == GFX_EVENT_EXIT || (event.event == GFX_EVENT_KEYBOARD && event.data == 27)) {
            break;
        }

        if (event.event == GFX_EVENT_KEYBOARD) {
            if (event.data == KEY_DOWN && first + shown < EDITOR_HELP_ROWS) {
                first++;
                redraw = true;
            } else if (event.data == KEY_UP && first > 0) {
                first--;
                redraw = true;
            }
        }
    }

    editor_help_open = 0;
}

/**
 * Opens the Help window in front. One already open may be behind the Editor, and a thread can't bring another's window
 * to the front, so that one is closed first: the new one opens in front of every other window
 */
void Editor::Help()
{
    if (editor_help_open) {
        editor_help_close = 1;
        invoke_syscall(SYSCALL_AWAIT_PROCESS, editor_help_pid, 0, 0);
        editor_help_close = 0;
    }

    editor_help_open = 1;
    Thread help_thread(__editor_help_thread, 0);
    editor_help_pid = help_thread.start(nullptr);
    if (editor_help_pid < 0) {
        editor_help_open = 0;
        drawStatusLine(COLOR_VGA_RED, "Failed to open Help window.");
    }
}

void Editor::EditorLoop()
{
    reDrawHeader();
    drawTreePane();
    reDraw(0, m_bufferHead);

    while (1) {
        struct gfx_event event;

        /* A drag held above or below the text scrolls a row each tick, so wait for events only that long */
        if (m_dragScroll != 0) {
            if (gfx_get_event(&event, GFX_EVENT_NONBLOCKING) < 0) {
                sleep(EDITOR_DRAG_SCROLL_MS);
                dragScrollTick();
                continue;
            }
        } else {
            gfx_get_event(&event, GFX_EVENT_BLOCKING);
        }

        /* What's kept on the status line goes at the next event: a key, a click, or a resize, which moves the line */
        m_keepStatus = 0;

        switch (event.event) {
        case GFX_EVENT_KEYBOARD:
            putChar(event.data, event.flags);
            break;

        case GFX_EVENT_MOUSE_DOWN:
        case GFX_EVENT_MOUSE_MOVE:
        case GFX_EVENT_MOUSE_UP: {
            m_mouseHeld = event.event != GFX_EVENT_MOUSE_UP;
            if (event.event == GFX_EVENT_MOUSE_DOWN) {
                m_pressOnTree = (int16_t)event.data < m_treeWidth - EDITOR_TREE_GRAB_LEFT;

                /* A press anywhere closes the prompt, as Esc does, then does what it does */
                if (m_prompt != EDITOR_PROMPT_NONE) {
                    closePrompt();
                    drawStatus();
                }
            }

            /* The tree's toggle, which presses Ctrl+B when it's clicked. A press on it isn't on a row either */
            bool toggled = false;
            if (treeToggleMouse(event, &toggled)) {
                m_pressOnTree = false;
                if (toggled) {
                    putChar(CTRLB);
                }
                break;
            }

            if (treeResizeMouse(event)) {
                break;
            }

            /**
             * The tree's scroll bar. A press on it isn't on a row, so the click after its release opens nothing. While
             * the tree is collapsed, the bar is still placed where it was, over the text, so it gets nothing
             */
            int tree_scroll = m_treeCollapsed ? 0 : treeView->mouse(event);
            if (tree_scroll & SCROLLBAR_TAKEN) {
                m_pressOnTree = false;
                if (tree_scroll & SCROLLBAR_SCROLLED) {
                    drawTreePane();
                }
                break;
            }

            /* The text's scroll bar, which moves the view and leaves the cursor where it is, as F2 and F3 do */
            int text_scroll = m_textBar.mouse(event);
            if (text_scroll & SCROLLBAR_TAKEN) {
                if (text_scroll & SCROLLBAR_SCROLLED) {
                    scrollToRow(m_textBar.first());
                    reDraw(0, m_bufferHead);
                } else if (text_scroll & SCROLLBAR_REDRAW) {
                    m_textBar.draw();
                }
                break;
            }

            /* A button clicked presses its key, so it does just what the key does */
            int clicked = EDITOR_BUTTON_NONE;
            if (!buttonMouse(event, &clicked)) {
                handleMouse(event);
            } else if (clicked != EDITOR_BUTTON_NONE) {
                putChar(editor_button_keys[clicked]);
            }
            break;
        }

        case GFX_EVENT_MOUSE:
            if (m_pressOnTree && event.data < m_treeWidth) {
                const char* file = treeView->click(event.data, event.data2);
                drawTreePane();
                if (file != nullptr) {
                    Open((char*)file);
                }
                reDrawHeader();
                reDraw(0, m_bufferHead);
            }
            break;

        case GFX_EVENT_RESOLUTION:
            applyResolution(event.data, event.data2);
            reDrawHeader();
            drawTreePane();
            reDraw(0, m_bufferHead);
            break;

        case GFX_EVENT_EXIT:
            if (Quit()) {
                return;
            }
            break;

        default:
            break;
        }
    }
}

/* Moves the cursor down a line, to the column it had before moving up or down, or the end of a shorter line. @return false on the last line */
bool Editor::cursorDown()
{
    int currentStart = lineStart(m_bufferEdit);

    if (m_preferredColumn < 0) {
        m_preferredColumn = lineColumn(m_bufferEdit);
    }

    int currentEnd = lineEnd(currentStart);
    if (currentEnd >= m_bufferHead || m_textBuffer[currentEnd] != '\n') {
        return false;
    }

    m_bufferEdit = indexAtColumn(currentEnd + 1, m_preferredColumn);
    return true;
}

/* Moves the cursor up a line, as cursorDown moves it down. @return false on the first line */
bool Editor::cursorUp()
{
    int currentStart = lineStart(m_bufferEdit);

    if (m_preferredColumn < 0) {
        m_preferredColumn = lineColumn(m_bufferEdit);
    }

    if (currentStart == 0) {
        return false;
    }

    m_bufferEdit = indexAtColumn(lineStart(currentStart - 1), m_preferredColumn);
    return true;
}

/* The keys that move the cursor, which select with Shift */
static bool isMovementKey(unsigned char c)
{
    switch (c) {
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
    case KEY_PAGE_UP:
    case KEY_PAGE_DOWN:
        return true;
    default:
        return false;
    }
}

/* Handles a key typed, with the modifier keys held, KEY_MOD_* from keyboard.h. Its edits make an undo step, or join the last */
void Editor::putChar(unsigned char c, int modifiers)
{
    /* A prompt on the status line takes every key while it's open. Opening it ended the undo step, as any other key does */
    if (m_prompt != EDITOR_PROMPT_NONE) {
        promptKey(c, modifiers);
        return;
    }

    int kind = EDITOR_UNDO_CLOSED;
    if (c == '\b') {
        kind = EDITOR_UNDO_BACKSPACE;
    } else if (c == KEY_DELETE) {
        kind = EDITOR_UNDO_DELETE;
    } else if (c == '\n' || c == '\r' || c == '\t' || c == CTRLX || c == CTRLV) {
        kind = EDITOR_UNDO_ALONE;
    } else if (c >= 32 && c <= 126) {
        kind = EDITOR_UNDO_TYPING;
    }

    undoKeyStart(kind);
    handleKey(c, modifiers);
    undoKeyEnd();
}

/* What a key does, for putChar */
void Editor::handleKey(unsigned char c, int modifiers)
{
    if (c == '\r') {
        c = '\n';
    }

    /* A move with Shift extends the selection from where it started; one without ends it */
    if (isMovementKey(c)) {
        if (modifiers & KEY_MOD_SHIFT) {
            if (m_selAnchor < 0) {
                m_selAnchor = m_bufferEdit;
            }
        } else if (hasSelection() && (c == KEY_LEFT || c == KEY_RIGHT)) {
            /* To the selection's start or end, not a character past it */
            int start = 0;
            int end = 0;
            selectionRange(start, end);
            m_bufferEdit = c == KEY_LEFT ? start : end;
            m_preferredColumn = -1;
            clearSelection();
            ensureCursorVisible(visibleColumns(), visibleLines());
            reDraw(0, m_bufferHead);
            return;
        } else {
            clearSelection();
        }
    }

    /* Typing, Enter, Tab, Backspace and Delete replace or delete the selection */
    bool edits = c == '\b' || c == KEY_DELETE || c == '\n' || c == '\t' || (c >= 32 && c <= 126);
    if (m_readOnly && (edits || c == CTRLX || c == CTRLV)) {
        /* Typing into a file that can't be saved would lose the typing */
        drawStatusLine(COLOR_VGA_RED, "Read-only: too big for the Editor to save.");
        return;
    }
    bool replaced = edits && deleteSelection();
    if (replaced) {
        if (c == '\b' || c == KEY_DELETE) {
            ensureCursorVisible(visibleColumns(), visibleLines());
            reDraw(0, m_bufferHead);
            return;
        }
    } else if (edits) {
        clearSelection();
    }

    bool buffer_changed = false;

    switch (c) {
    case CTRLA:
        m_selAnchor = 0;
        m_bufferEdit = m_bufferHead;
        m_preferredColumn = -1;
        break;

    case 27:
        /* Esc */
        clearSelection();
        break;

    case CTRLC:
        copySelection();
        break;

    case CTRLX:
        if (copySelection()) {
            deleteSelection();
        }
        break;

    case CTRLV:
        paste();
        break;

    case CTRLZ:
        if (modifiers & KEY_MOD_SHIFT) {
            redo();
        } else {
            undo();
        }
        break;

    case CTRLY:
        redo();
        break;

    case '\b':
        if (m_bufferEdit <= 0 || m_bufferHead <= 0) {
            return;
        }

        /* Right after a {, the } that typing it added goes too */
        if (m_textBuffer[m_bufferEdit - 1] == '{' && autoBraceAt(m_bufferEdit) >= 0) {
            if (bufferDeleteRange(m_bufferEdit - 1, 2)) {
                m_bufferEdit--;
                m_dirty = 1;
                m_preferredColumn = -1;
                buffer_changed = true;
            }
            break;
        }

        if (bufferDeleteChar(m_bufferEdit - 1)) {
            m_bufferEdit--;
            m_dirty = 1;
            m_preferredColumn = -1;
            buffer_changed = true;
        }
        break;

    case KEY_LEFT:
        if (m_bufferEdit > 0) {
            m_bufferEdit--;
        }
        m_preferredColumn = -1;
        break;

    case KEY_RIGHT:
        if (m_bufferEdit < m_bufferHead) {
            m_bufferEdit++;
        }
        m_preferredColumn = -1;
        break;

    case KEY_DOWN:
        cursorDown();
        break;

    case KEY_UP:
        cursorUp();
        break;

    case KEY_HOME:
        m_bufferEdit = lineStart(m_bufferEdit);
        m_preferredColumn = -1;
        break;

    case KEY_END:
        m_bufferEdit = lineEnd(m_bufferEdit);
        m_preferredColumn = -1;
        break;

    case KEY_PAGE_DOWN:
    case KEY_PAGE_UP: {
        /* A screenful less a row, so the row at the edge stays in view. Past the last line, to the end of the text */
        int page = visibleLines() - 1;
        if (page < 1) {
            page = 1;
        }

        /* The view moves a page too, but down only as far as there's text below it */
        bool down = c == KEY_PAGE_DOWN;
        int cols = visibleColumns();
        int below = rowsFrom(viewStartIndex(cols), cols) - visibleLines();
        scroll(down ? minInt(page, below > 0 ? below : 0) : -page);
        for (int n = 0; n < page; n++) {
            if (!(down ? cursorDown() : cursorUp())) {
                m_bufferEdit = down ? m_bufferHead : 0;
                m_preferredColumn = -1;
                break;
            }
        }
        break;
    }

    case KEY_DELETE:
        if (m_bufferEdit >= m_bufferHead) {
            return;
        }

        if (bufferDeleteChar(m_bufferEdit)) {
            m_dirty = 1;
            m_preferredColumn = -1;
            buffer_changed = true;
        }
        break;

    case KEY_F3:
        scroll(1);
        reDraw(0, m_bufferHead);
        return;

    case KEY_F2:
        scroll(-1);
        reDraw(0, m_bufferHead);
        return;

    case CTRLH:
        m_preferredColumn = -1;
        Help();
        break;

    case CTRLB:
        if (!m_mouseHeld) {
            setTreeCollapsed(!m_treeCollapsed);
        }
        return;

    case CTRLN:
        m_preferredColumn = -1;
        New();
        break;

    case CTRLS:
        m_preferredColumn = -1;
        Save();
        break;

    case CTRLG:
        /* Moves nothing until Enter (promptKey) */
        openPrompt(EDITOR_PROMPT_LINE);
        return;

    case CTRLF:
        openPrompt(EDITOR_PROMPT_FIND);
        return;

    default:
        if (c == '\t') {
            for (int n = 0; n < EDITOR_TAB_WIDTH; n++) {
                if (!bufferInsertChar(m_bufferEdit, ' ')) {
                    break;
                }
                m_bufferEdit++;
                buffer_changed = true;
            }
            if (buffer_changed) {
                m_dirty = 1;
                m_preferredColumn = -1;
            }
            break;
        }

        if (c == 0) {
            return;
        }

        if (c != '\n' && (c < 32 || c > 126)) {
            return;
        }

        /* A } typed where there's one that typing { added steps over it, so a block typed in full ends with one (#119) */
        if (c == '}' && !replaced && m_textBuffer[m_bufferEdit] == '}') {
            int brace = autoBraceAt(m_bufferEdit);
            if (brace >= 0) {
                for (int i = brace; i < m_autoBraceCount - 1; i++) {
                    m_autoBraces[i] = m_autoBraces[i + 1];
                }
                m_autoBraceCount--;
                m_bufferEdit++;
                m_preferredColumn = -1;
                break;
            }
        }

        if (!bufferInsertChar(m_bufferEdit, c)) {
            return;
        }

        m_bufferEdit++;
        buffer_changed = true;

        if (c == '{' && bufferInsertChar(m_bufferEdit, '}')) {
            /* Kept track of until it's typed over or deleted. With too many blocks open, the oldest is forgotten */
            if (m_autoBraceCount == EDITOR_AUTO_BRACES_MAX) {
                memmove(&m_autoBraces[0], &m_autoBraces[1], (EDITOR_AUTO_BRACES_MAX - 1) * sizeof(int));
                m_autoBraceCount--;
            }
            m_autoBraces[m_autoBraceCount++] = m_bufferEdit;
        }

        m_dirty = 1;
        m_preferredColumn = -1;
        break;
    }

    if (m_bufferEdit < 0) {
        m_bufferEdit = 0;
    }

    if (m_bufferEdit > m_bufferHead) {
        m_bufferEdit = m_bufferHead;
    }

    ensureCursorVisible(visibleColumns(), visibleLines());
    reDraw(0, m_bufferHead);
}

extern "C" int main(int argc, char* argv[])
{
    Editor* editor = new Editor();

    /* Without a file, an untitled text, which the first save names */
    if (argc > 1) {
        editor->Open(argv[1]);
    }

    editor->EditorLoop();
    return 0;
}
