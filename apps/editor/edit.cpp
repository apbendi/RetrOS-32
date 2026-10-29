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
        popup.drawText(10, 56, "Enter=Save  Esc/F4=Cancel", COLOR_VGA_MEDIUM_DARK_GRAY);

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

            if (event.data == 27 || event.data == KEY_F4) {
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
    m_x = 0;
    m_y = 0;
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
    m_keepStatus = 0;
    scrollY = 0;
    m_scrollWrap = 0;
    m_textColor = COLOR_TEXT;
    c_width = EDITOR_DEFAULT_RIGHT_PANE_WIDTH;
    c_height = EDITOR_DEFAULT_HEIGHT;
    m_ropeHead = nullptr;
    m_ropeTail = nullptr;

    m_textBuffer = (unsigned char*) malloc(m_bufferSize);
    if (m_textBuffer == nullptr) {
        exit();
    }

    memset(m_textBuffer, 0, m_bufferSize);
    memset(m_currentPath, 0, sizeof(m_currentPath));

    treeView = new TreeView(0, 0, TREE_VIEW_WIDTH, c_height);

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

    ropeClear();

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
    return TREE_VIEW_WIDTH + gutterWidth() + TEXT_GAP_WIDTH;
}

int Editor::textClipWidth() const
{
    int width = c_width - gutterWidth() - TEXT_GAP_WIDTH - TEXT_RIGHT_PADDING;
    if (width < 8) {
        width = 8;
    }
    return width;
}

int Editor::visibleLines() const
{
    int h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;
    if (h < 8) {
        return 1;
    }

    return h / 8;
}

int Editor::visibleColumns() const
{
    int w = textClipWidth();
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
    int len = line_end - line_start;

    if (len <= 0) {
        return 1;
    }

    if (cols < 1) {
        cols = 1;
    }

    return ((len - 1) / cols) + 1;
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
    if (cols < 1) {
        cols = 1;
    }

    int idx = clampInt(row_start_index, 0, m_bufferHead);
    if (idx >= m_bufferHead) {
        return m_bufferHead;
    }

    int col = 0;
    while (idx < m_bufferHead) {
        unsigned char ch = m_textBuffer[idx];
        if (ch == '\n') {
            return idx + 1;
        }

        idx++;
        col++;
        if (col >= cols) {
            /* As reDraw lays rows out: a newline after a full row ends it, rather than making a row of its own (#130) */
            return idx < m_bufferHead && m_textBuffer[idx] == '\n' ? idx + 1 : idx;
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
    int row = 0;
    int col = 0;

    if (m_bufferEdit < view_start) {
        *out_row = -1;
        *out_col = 0;
        return;
    }

    /* As reDraw lays rows out: a full row continues on the next only when a character other than a newline follows */
    int limit = clampInt(m_bufferEdit, 0, m_bufferHead);
    for (int i = view_start; i < limit; i++) {
        unsigned char ch = m_textBuffer[i];
        if (ch == '\n') {
            row++;
            col = 0;
            continue;
        }

        if (col >= cols) {
            row++;
            col = 0;
        }
        col++;
    }

    /**
     * After a full row, the cursor is on the next character, at the start of the next row. Before a newline or the
     * end of the text, it stays past the row's end, at column cols, where reDraw draws it in the right margin
     */
    if (col >= cols && limit < m_bufferHead && m_textBuffer[limit] != '\n') {
        row++;
        col = 0;
    }

    *out_row = row;
    *out_col = col;
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
    int gutter_x = TREE_VIEW_WIDTH;
    int gutter_w = gutterWidth();

    int text_h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;
    if (text_h < 8) {
        text_h = 8;
    }

    gfx_draw_rectangle(gutter_x, HEADER_HEIGHT, gutter_w, text_h, COLOR_BG);
    /* Below the header, which isn't redrawn when the gutter widens, so the line would stay there at its old place */
    gfx_draw_line(gutter_x + gutter_w - 1, HEADER_HEIGHT, gutter_x + gutter_w - 1, c_height, COLOR_VGA_MEDIUM_GRAY);

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

        gfx_draw_format_text(x, HEADER_HEIGHT + (row * 8), COLOR_VGA_MEDIUM_GRAY, "%d", line_no);
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
     * The whole row, under the gutter and the margin too, except the gutter's line, which drawLineNumbers draws down
     * to the bottom. The text starts further right as the gutter widens, and its old start would stay behind (#144)
     */
    int line_x = x - TEXT_GAP_WIDTH - 1;
    gfx_draw_rectangle(TREE_VIEW_WIDTH, y, line_x - TREE_VIEW_WIDTH, STATUS_HEIGHT, COLOR_BG);
    gfx_draw_rectangle(line_x + 1, y, TREE_VIEW_WIDTH + c_width - (line_x + 1), STATUS_HEIGHT, COLOR_BG);

    int max_chars = w / 8;
    if (max_chars < 1) {
        return;
    }

    int len = strlen(msg);
    if (len > max_chars) {
        msg[max_chars] = 0;
    }

    gfx_draw_format_text(x, y, color, "%s", msg);
    m_keepStatus = color == COLOR_VGA_RED;
}

void Editor::reDrawHeader()
{
    gfx_draw_rectangle(TREE_VIEW_WIDTH, 0, c_width, HEADER_HEIGHT, COLOR_BG);
    drawHeaderTable(TREE_VIEW_WIDTH, c_width);

    const char* left_label = "F1 Save  F4 Open";
    const char* right_label = "F9 Help";

    int content_left = TREE_VIEW_WIDTH + 4;
    int content_right = TREE_VIEW_WIDTH + c_width - 4;

    int left_w = strlen(left_label) * 8;
    int right_w = strlen(right_label) * 8;

    int right_x = content_right - right_w;
    if (right_x < content_left + left_w + 8) {
        left_label = "F1 Save";
        left_w = strlen(left_label) * 8;
    }

    right_w = strlen(right_label) * 8;
    right_x = content_right - right_w;
    if (right_x < content_left + left_w + 8) {
        right_label = "F9";
        right_w = strlen(right_label) * 8;
        right_x = content_right - right_w;
    }

    if (right_x < content_left + left_w + 8) {
        left_label = "";
        left_w = 0;
    }

    if (right_x < content_left) {
        right_x = content_left;
    }

    if (left_w > 0) {
        gfx_draw_format_text(content_left, 2, COLOR_BLACK, "%s", left_label);
    }
    gfx_draw_format_text(right_x, 2, COLOR_BLACK, "%s", right_label);
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
            gfx_draw_rectangle(text_x + (run_from * 8), HEADER_HEIGHT + (run_row * 8), (run_to - run_from) * 8, 8, COLOR_SELECTED);
        }
        run_row = -1;
    };

    int row = 0;
    int col = 0;
    for (int i = view_start; i < end && row < rows; i++) {
        unsigned char ch = m_textBuffer[i];
        if (ch != '\n' && col >= cols) {
            row++;
            col = 0;
            if (row >= rows) {
                break;
            }
        }

        /* A newline after a full row has no cell left on it */
        if (i >= start && col < cols) {
            if (run_row != row) {
                drawRun();
                run_row = row;
                run_from = col;
            }
            run_to = col + 1;
        }

        if (ch == '\n') {
            row++;
            col = 0;
        } else {
            col++;
        }
    }
    drawRun();
}

/**
 * The boundary between characters nearest x, y in the window's content, with the text laid out as reDraw draws it.
 * Above the text is its first row and below it its last. Left of the text is a row's start, and past a row's end is
 * its end. Past the text's last row is the text's end.
 */
int Editor::indexAtPoint(int x, int y) const
{
    int cols = visibleColumns();
    int rows = visibleLines();

    int row = y < HEADER_HEIGHT ? 0 : (y - HEADER_HEIGHT) / 8;
    if (row >= rows) {
        row = rows - 1;
    }

    /* The nearer edge of the character under x */
    int dx = x - textStartX();
    int col = dx < 0 ? 0 : (dx + 4) / 8;
    if (col > cols) {
        col = cols;
    }

    int r = 0;
    int c = 0;
    for (int i = viewStartIndex(cols); i < m_bufferHead; i++) {
        unsigned char ch = m_textBuffer[i];
        if (ch != '\n' && c >= cols) {
            r++;
            c = 0;
        }

        /* Past the end of a full row is the start of the next */
        if (r > row || (r == row && c >= col)) {
            return i;
        }

        if (ch == '\n') {
            if (r == row) {
                return i;
            }
            r++;
            c = 0;
        } else {
            c++;
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
        if (x < TREE_VIEW_WIDTH || y < HEADER_HEIGHT || y >= text_bottom) {
            return;
        }

        /* Typing after a click is a new undo step, even where the last one ended */
        m_undoJoin = EDITOR_UNDO_CLOSED;

        /* A double click selects the word under the mouse, and a triple its line. It's no drag, so a shaking mouse can't undo it */
        if (GFX_EVENT_CLICKS(event.flags) >= 2) {
            selectAround(indexAtPoint(x - 4, y), GFX_EVENT_CLICKS(event.flags) == 3);
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

    int y = m_dragScroll < 0 ? HEADER_HEIGHT : HEADER_HEIGHT + ((rows - 1) * 8);
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

    /* From the gutter's line to the window's edge, so the gap before the text and the margin after it are painted
     * too. Otherwise a resize leaves them black, and a narrower gutter leaves its old digits in the gap (#144) */
    int pane_x = text_x - TEXT_GAP_WIDTH;
    gfx_draw_rectangle(pane_x, HEADER_HEIGHT, TREE_VIEW_WIDTH + c_width - pane_x, text_h, COLOR_BG);

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

    int row = 0;
    int col = 0;
    if (draw_rows > 0) {
        row_lines[0] = line_no;
    }

    setColor(COLOR_TEXT);

    for (int i = view_start; i < m_bufferHead && row < draw_rows; i++) {
        unsigned char ch = m_textBuffer[i];

        if (ch == '\n') {
            row++;
            col = 0;
            line_no++;
            if (row < draw_rows) {
                row_lines[row] = line_no;
            }
            continue;
        }

        if (col >= cols) {
            row++;
            col = 0;
            if (row >= draw_rows) {
                break;
            }
            row_lines[row] = line_no;
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

        gfx_draw_char(text_x + (col * 8), HEADER_HEIGHT + (row * 8), ch, m_textColor);
        col++;
    }

    drawLineNumbers(row_lines, draw_rows);

    int cursor_row = 0;
    int cursor_col = 0;
    cursorFromViewStart(view_start, cols, &cursor_row, &cursor_col);

    if (cursor_row >= 0 && cursor_row < draw_rows) {
        int cx = text_x + (cursor_col * 8);
        int cy = HEADER_HEIGHT + (cursor_row * 8);

        /* At the end of a full row, the cursor is past it, in the right margin, which is narrower than a character */
        int right = TREE_VIEW_WIDTH + c_width;
        if (cx > text_x + (cols * 8) - 8 && cx < right) {
            gfx_draw_rectangle(cx, cy, minInt(8, right - cx), 8, COLOR_VGA_LIGHT_GRAY);
        }

        if (cx <= text_x + (cols * 8) - 8) {
            gfx_draw_rectangle(cx, cy, 8, 8, COLOR_VGA_LIGHT_GRAY);

            if (m_bufferEdit < m_bufferHead && m_textBuffer[m_bufferEdit] != '\n') {
                unsigned char ch = m_textBuffer[m_bufferEdit];
                if (ch >= 32 && ch <= 126) {
                    if (isWordChar(ch)) {
                        highlightWordAt(m_bufferEdit);
                    } else {
                        setColor(COLOR_TEXT);
                    }
                    gfx_draw_char(cx, cy, ch, m_textColor);
                }
            }
        }
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

void Editor::drawChar(unsigned char c, color_t bg)
{
    int cols = visibleColumns();
    int rows = visibleLines();

    if (cols < 1 || rows < 1) {
        return;
    }

    if (c == '\n') {
        m_x = 0;
        m_y++;
        return;
    }

    if (m_x >= cols) {
        m_x = 0;
        m_y++;
    }

    if (m_y < 0 || m_y >= rows) {
        return;
    }

    int drawX = textStartX() + (m_x * 8);
    int drawY = HEADER_HEIGHT + (m_y * 8);

    gfx_draw_rectangle(drawX, drawY, 8, 8, bg);

    if (c < 32 || c > 126) {
        c = '?';
    }

    gfx_draw_char(drawX, drawY, c, m_textColor);
    m_x++;
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

void Editor::getCursorLineCol(int& line, int& col) const
{
    line = 1;
    col = 1;

    int limit = clampInt(m_bufferEdit, 0, m_bufferHead);
    for (int i = 0; i < limit; i++) {
        if (m_textBuffer[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
}

void Editor::Lex()
{
}

bool Editor::ropeLocate(int index, struct rope_node** out_node, int* out_offset) const
{
    if (index < 0 || index > m_bufferHead) {
        return false;
    }

    int pos = 0;
    struct rope_node* node = m_ropeHead;
    while (node != nullptr) {
        if (index <= pos + node->len) {
            *out_node = node;
            *out_offset = index - pos;
            return true;
        }

        pos += node->len;
        node = node->next;
    }

    if (index == pos) {
        *out_node = nullptr;
        *out_offset = 0;
        return true;
    }

    return false;
}

Editor::rope_node* Editor::ropeCreateNode()
{
    struct rope_node* node = (struct rope_node*) malloc(sizeof(struct rope_node));
    if (node == nullptr) {
        return nullptr;
    }

    memset(node, 0, sizeof(struct rope_node));
    node->prev = nullptr;
    node->next = nullptr;
    node->len = 0;
    return node;
}

void Editor::ropeDetachNode(struct rope_node* node)
{
    if (node == nullptr) {
        return;
    }

    if (node->prev != nullptr) {
        node->prev->next = node->next;
    } else {
        m_ropeHead = node->next;
    }

    if (node->next != nullptr) {
        node->next->prev = node->prev;
    } else {
        m_ropeTail = node->prev;
    }

    free(node);
}

void Editor::ropeTryMerge(struct rope_node* node)
{
    if (node == nullptr) {
        return;
    }

    if (node->next != nullptr) {
        struct rope_node* next = node->next;
        if (node->len + next->len <= ROPE_CHUNK_SIZE) {
            memcpy(&node->data[node->len], next->data, next->len);
            node->len += next->len;
            ropeDetachNode(next);
        }
    }

    if (node->prev != nullptr) {
        struct rope_node* prev = node->prev;
        if (prev->len + node->len <= ROPE_CHUNK_SIZE) {
            memcpy(&prev->data[prev->len], node->data, node->len);
            prev->len += node->len;
            ropeDetachNode(node);
        }
    }
}

void Editor::ropeClear()
{
    struct rope_node* node = m_ropeHead;
    while (node != nullptr) {
        struct rope_node* next = node->next;
        free(node);
        node = next;
    }

    m_ropeHead = nullptr;
    m_ropeTail = nullptr;
    m_autoBraceCount = 0;
    undoClear();
}

/* Copies count characters from index in the rope to out, which ropeDeleteRange can't do once they're gone */
void Editor::ropeCopy(int index, int count, unsigned char* out) const
{
    struct rope_node* node = nullptr;
    int offset = 0;
    if (count <= 0 || !ropeLocate(index, &node, &offset)) {
        return;
    }

    while (node != nullptr && count > 0) {
        int take = minInt(count, node->len - offset);
        if (take > 0) {
            memcpy(out, &node->data[offset], take);
            out += take;
            count -= take;
        }
        node = node->next;
        offset = 0;
    }
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

bool Editor::ropeLoadFromBuffer(const unsigned char* data, int len)
{
    ropeClear();

    if (data == nullptr || len <= 0) {
        m_bufferHead = 0;
        return true;
    }

    int pos = 0;
    while (pos < len) {
        int chunk = len - pos;
        if (chunk > ROPE_CHUNK_SIZE) {
            chunk = ROPE_CHUNK_SIZE;
        }

        struct rope_node* node = ropeCreateNode();
        if (node == nullptr) {
            ropeClear();
            return false;
        }

        memcpy(node->data, &data[pos], chunk);
        node->len = chunk;

        if (m_ropeTail == nullptr) {
            m_ropeHead = node;
            m_ropeTail = node;
        } else {
            m_ropeTail->next = node;
            node->prev = m_ropeTail;
            m_ropeTail = node;
        }

        pos += chunk;
    }

    m_bufferHead = len;
    return true;
}

bool Editor::ropeSyncToBuffer()
{
    int pos = 0;
    struct rope_node* node = m_ropeHead;
    while (node != nullptr) {
        int to_copy = node->len;
        if (to_copy < 0) {
            to_copy = 0;
        }

        if (pos + to_copy > m_bufferSize - 1) {
            to_copy = (m_bufferSize - 1) - pos;
            if (to_copy < 0) {
                to_copy = 0;
            }
        }

        if (to_copy > 0) {
            memcpy(&m_textBuffer[pos], node->data, to_copy);
            pos += to_copy;
        }

        if (pos >= m_bufferSize - 1) {
            break;
        }

        node = node->next;
    }

    m_bufferHead = pos;
    m_textBuffer[m_bufferHead] = 0;

    if (m_bufferEdit > m_bufferHead) {
        m_bufferEdit = m_bufferHead;
    }

    return true;
}

bool Editor::ropeInsertChar(int index, unsigned char c)
{
    if (!ropeAddChar(index, c)) {
        return false;
    }

    autoBracesInserted(index, 1);
    undoInserted(index, &c, 1);
    return true;
}

/* Inserts c at index in the rope. ropeInsertChar also moves the } kept in m_autoBraces, and records the edit for undo */
bool Editor::ropeAddChar(int index, unsigned char c)
{
    if (m_bufferHead >= m_bufferSize - 1) {
        return false;
    }

    struct rope_node* node = nullptr;
    int offset = 0;
    if (!ropeLocate(index, &node, &offset)) {
        return false;
    }

    if (m_ropeHead == nullptr) {
        struct rope_node* fresh = ropeCreateNode();
        if (fresh == nullptr) {
            return false;
        }

        fresh->data[0] = c;
        fresh->len = 1;
        m_ropeHead = fresh;
        m_ropeTail = fresh;
        m_bufferHead++;
        return true;
    }

    if (node == nullptr) {
        if (m_ropeTail->len < ROPE_CHUNK_SIZE) {
            m_ropeTail->data[m_ropeTail->len++] = c;
        } else {
            struct rope_node* fresh = ropeCreateNode();
            if (fresh == nullptr) {
                return false;
            }

            fresh->data[0] = c;
            fresh->len = 1;
            fresh->prev = m_ropeTail;
            m_ropeTail->next = fresh;
            m_ropeTail = fresh;
        }

        m_bufferHead++;
        return true;
    }

    if (offset == node->len && node->len < ROPE_CHUNK_SIZE) {
        node->data[node->len++] = c;
        m_bufferHead++;
        return true;
    }

    if (offset == 0 && node->len < ROPE_CHUNK_SIZE) {
        memmove(&node->data[1], &node->data[0], node->len);
        node->data[0] = c;
        node->len++;
        m_bufferHead++;
        return true;
    }

    if (offset > 0 && offset < node->len) {
        int right_len = node->len - offset;

        struct rope_node* right = ropeCreateNode();
        if (right == nullptr) {
            return false;
        }

        memcpy(right->data, &node->data[offset], right_len);
        right->len = right_len;

        node->len = offset;

        right->next = node->next;
        right->prev = node;
        if (node->next != nullptr) {
            node->next->prev = right;
        } else {
            m_ropeTail = right;
        }
        node->next = right;

        if (node->len < ROPE_CHUNK_SIZE) {
            node->data[node->len++] = c;
        } else {
            struct rope_node* mid = ropeCreateNode();
            if (mid == nullptr) {
                return false;
            }

            mid->data[0] = c;
            mid->len = 1;

            mid->prev = node;
            mid->next = right;
            node->next = mid;
            right->prev = mid;
        }

        m_bufferHead++;
        return true;
    }

    struct rope_node* fresh = ropeCreateNode();
    if (fresh == nullptr) {
        return false;
    }

    fresh->data[0] = c;
    fresh->len = 1;

    if (offset <= 0) {
        fresh->prev = node->prev;
        fresh->next = node;
        if (node->prev != nullptr) {
            node->prev->next = fresh;
        } else {
            m_ropeHead = fresh;
        }
        node->prev = fresh;
    } else {
        fresh->next = node->next;
        fresh->prev = node;
        if (node->next != nullptr) {
            node->next->prev = fresh;
        } else {
            m_ropeTail = fresh;
        }
        node->next = fresh;
    }

    m_bufferHead++;
    return true;
}

bool Editor::ropeDeleteChar(int index)
{
    if (index < 0 || index >= m_bufferHead) {
        return false;
    }

    struct rope_node* node = nullptr;
    int offset = 0;
    if (!ropeLocate(index, &node, &offset)) {
        return false;
    }

    if (node == nullptr) {
        return false;
    }

    if (offset == node->len && node->next != nullptr) {
        node = node->next;
        offset = 0;
    }

    if (offset < 0 || offset >= node->len) {
        return false;
    }

    undoDeleting(index, 1);
    memmove(&node->data[offset], &node->data[offset + 1], node->len - offset - 1);
    node->len--;
    m_bufferHead--;
    autoBracesDeleted(index, 1);

    if (node->len <= 0) {
        ropeDetachNode(node);
    } else {
        ropeTryMerge(node);
    }

    if (m_bufferHead < 0) {
        m_bufferHead = 0;
    }

    return true;
}

/* Deletes count characters from index, cutting each node they're in once. @return false if they aren't all in the text */
bool Editor::ropeDeleteRange(int index, int count)
{
    if (index < 0 || count < 0 || index + count > m_bufferHead) {
        return false;
    }
    if (count == 0) {
        return true;
    }

    struct rope_node* node = nullptr;
    int offset = 0;
    if (!ropeLocate(index, &node, &offset) || node == nullptr) {
        return false;
    }

    /* The check above means they're all there, so they all go */
    undoDeleting(index, count);

    /* At the end of a node, the first character to go is in the next */
    int left = count;
    while (node != nullptr && left > 0) {
        struct rope_node* next = node->next;
        int take = minInt(left, node->len - offset);
        if (take > 0) {
            memmove(&node->data[offset], &node->data[offset + take], node->len - offset - take);
            node->len -= take;
            left -= take;
            m_bufferHead -= take;
            if (node->len == 0) {
                ropeDetachNode(node);
            }
        }
        node = next;
        offset = 0;
    }

    autoBracesDeleted(index, count - left);

    /* Joins the nodes either side of the gap, if they fit in one */
    if (ropeLocate(index, &node, &offset) && node != nullptr) {
        ropeTryMerge(node);
    }

    return left == 0;
}

/**
 * Inserts len characters at index, in nodes of their own, splitting the node index is in the middle of.
 * @return false, having changed nothing, without the room in the buffer or the memory for the nodes
 */
bool Editor::ropeInsertText(int index, const unsigned char* data, int len)
{
    if (len <= 0) {
        return true;
    }
    /* Undo and redo put the new text in before taking the old out, so the rope can hold more than the buffer meanwhile (undoApply) */
    if (index < 0 || index > m_bufferHead || (!m_undoing && m_bufferHead + len > m_bufferSize - 1)) {
        return false;
    }

    /* The new nodes first, so running out of memory leaves the rope as it was */
    struct rope_node* first = nullptr;
    struct rope_node* last = nullptr;
    for (int pos = 0; pos < len; pos += ROPE_CHUNK_SIZE) {
        struct rope_node* fresh = ropeCreateNode();
        if (fresh == nullptr) {
            while (first != nullptr) {
                struct rope_node* next = first->next;
                free(first);
                first = next;
            }
            return false;
        }

        fresh->len = minInt(ROPE_CHUNK_SIZE, len - pos);
        memcpy(fresh->data, &data[pos], fresh->len);
        fresh->prev = last;
        if (last != nullptr) {
            last->next = fresh;
        } else {
            first = fresh;
        }
        last = fresh;
    }

    struct rope_node* node = nullptr;
    int offset = 0;
    ropeLocate(index, &node, &offset);

    /* They go between prev and next. An empty rope has neither */
    struct rope_node* prev = nullptr;
    struct rope_node* next = nullptr;
    if (node != nullptr && offset == 0) {
        prev = node->prev;
        next = node;
    } else if (node != nullptr && offset == node->len) {
        prev = node;
        next = node->next;
    } else if (node != nullptr) {
        struct rope_node* right = ropeCreateNode();
        if (right == nullptr) {
            while (first != nullptr) {
                struct rope_node* following = first->next;
                free(first);
                first = following;
            }
            return false;
        }

        right->len = node->len - offset;
        memcpy(right->data, &node->data[offset], right->len);
        node->len = offset;

        right->prev = node;
        right->next = node->next;
        if (node->next != nullptr) {
            node->next->prev = right;
        } else {
            m_ropeTail = right;
        }
        node->next = right;

        prev = node;
        next = right;
    }

    first->prev = prev;
    last->next = next;
    if (prev != nullptr) {
        prev->next = first;
    } else {
        m_ropeHead = first;
    }
    if (next != nullptr) {
        next->prev = last;
    } else {
        m_ropeTail = last;
    }
    m_bufferHead += len;
    autoBracesInserted(index, len);
    undoInserted(index, data, len);

    /* Joins the nodes at either end, if they fit in one */
    if (ropeLocate(index, &node, &offset) && node != nullptr) {
        ropeTryMerge(node);
    }
    if (ropeLocate(index + len, &node, &offset) && node != nullptr) {
        ropeTryMerge(node);
    }

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
    if (end == start || !ropeDeleteRange(start, end - start)) {
        return false;
    }

    ropeSyncToBuffer();
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
    if (!ropeInsertText(index, data, len)) {
        return false;
    }

    ropeSyncToBuffer();
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

/* Records for undo that len characters are about to go from index, copying them out of the rope while they're there */
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
    ropeCopy(index, ahead, del);
    ropeCopy(step->pos, len - ahead, &del[ahead + step->del]);
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
 * Changes the text as the step did, or back as it was. The text going in goes in after the text coming out, before
 * that's deleted, so running out of memory changes nothing. Meanwhile the rope can hold both, more than the buffer, so
 * ropeInsertText lets it while m_undoing, and the buffer is synced after.
 * @return false without the memory
 */
bool Editor::undoApply(const struct undo_step* step, bool redo)
{
    const unsigned char* del = (const unsigned char*)(step + 1);
    const unsigned char* ins = del + step->del;
    int out = redo ? step->del : step->ins;

    m_undoing = true;
    bool done = redo ? ropeInsertText(step->pos + out, ins, step->ins) : ropeInsertText(step->pos + out, del, step->del);
    if (done) {
        ropeDeleteRange(step->pos, out);
    }
    m_undoing = false;

    if (done) {
        ropeSyncToBuffer();
    }
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
    ropeClear();

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

    /* Unsaved changes count whether or not the file is on disk yet. Saving reopens m_fd, so it's closed after this */
    if (!SaveMsg()) {
        reDraw(0, m_bufferHead);
        return false;
    }

    static char absolute[FS_PATH_MAX];
    if (editor_absolute_path(path, absolute) < 0) {
        drawStatusLine(COLOR_VGA_RED, "Can't open %s: the path is too long.", path);
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

    if (read_error < 0 || !ropeLoadFromBuffer(m_textBuffer, m_bufferHead)) {
        if (m_fd >= 0) {
            fclose(m_fd);
            m_fd = -1;
        }
        /* An empty, untitled text. After a failed read the rope still holds the last file, which Reset empties too */
        setTitle("Editor");
        Reset();
        if (read_error < 0) {
            drawStatusLine(COLOR_VGA_RED, "Can't read %s: %s", absolute, error_get_string(read_error));
        } else {
            drawStatusLine(COLOR_VGA_RED, "Out of memory while loading file.");
        }
        return false;
    }

    m_bufferEdit = 0;
    scrollY = 0;
    m_scrollWrap = 0;
    m_fileSize = m_bufferHead;
    m_dirty = 0;
    m_preferredColumn = -1;
    m_selAnchor = -1;

    reDrawHeader();
    reDraw(0, m_bufferHead);
    if (is_new) {
        drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "New file: F1 saves it to disk.");
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

    if (!ropeSyncToBuffer()) {
        drawStatusLine(COLOR_VGA_RED, "Save failed.");
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
        treeView->refresh();
        treeView->drawTree(this);
    }
    reDraw(0, m_bufferHead);
    return true;
}

void Editor::applyResolution(int window_width, int window_height)
{
    c_width = window_width - TREE_VIEW_WIDTH;
    if (c_width < EDITOR_MIN_RIGHT_PANE_WIDTH) {
        c_width = EDITOR_MIN_RIGHT_PANE_WIDTH;
    }

    c_height = window_height;
    if (c_height < EDITOR_MIN_HEIGHT) {
        c_height = EDITOR_MIN_HEIGHT;
    }

    if (treeView != nullptr) {
        treeView->resize(TREE_VIEW_WIDTH, c_height);
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

void Editor::FileChooser()
{
    char selected_file[FS_PATH_MAX];
    memset(selected_file, 0, sizeof(selected_file));

    enum {
        CHOOSER_ACTION_NONE = 0,
        CHOOSER_ACTION_OPEN = 1,
        CHOOSER_ACTION_NEW = 2,
        CHOOSER_ACTION_CANCEL = 3
    };

    volatile int chooser_action = CHOOSER_ACTION_NONE;
    WidgetManager* chooser_widgets = nullptr;
    Button* open_button = nullptr;

    char open_label[] = "Open";
    char new_label[] = "New File";
    char cancel_label[] = "Cancel";

    int cached_panel_x = -1;
    int cached_panel_y = -1;
    int cached_panel_w = -1;
    int cached_panel_h = -1;

    auto freeChooserWidgets = [&]() {
        if (chooser_widgets != nullptr) {
            delete chooser_widgets;
            chooser_widgets = nullptr;
            open_button = nullptr;
        }
    };

    auto rebuildChooserWidgets = [&](int panel_x, int panel_y, int panel_w, int panel_h) {
        freeChooserWidgets();
        chooser_action = CHOOSER_ACTION_NONE;

        chooser_widgets = new WidgetManager();
        if (chooser_widgets == nullptr) {
            return;
        }

        int button_h = 14;
        int button_w = 82;
        int button_gap = 6;

        if (panel_w >= 170) {
            if ((button_w * 3) + (button_gap * 2) > panel_w - 8) {
                button_w = (panel_w - 8 - (button_gap * 2)) / 3;
                if (button_w < 44) {
                    button_w = 44;
                }
            }

            int content_w = (button_w * 3) + (button_gap * 2);
            int layout_x = panel_x + (panel_w - content_w) / 2;
            if (layout_x < panel_x + 2) {
                layout_x = panel_x + 2;
            }

            int layout_y = panel_y + panel_h - button_h - 8;
            if (layout_y < panel_y + 18) {
                layout_y = panel_y + 18;
            }

            Layout* actions = new Layout(layout_x, layout_y, content_w + 4, button_h + 4, HORIZONTAL, LAYOUT_FLAG_NONE);
            if (actions == nullptr) {
                return;
            }

            open_button = new Button(button_w, button_h, open_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_OPEN;
            }));
            Button* new_button = new Button(button_w, button_h, new_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_NEW;
            }));
            Button* cancel_button = new Button(button_w, button_h, cancel_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_CANCEL;
            }));
            if (open_button == nullptr || new_button == nullptr || cancel_button == nullptr) {
                if (open_button != nullptr) {
                    delete open_button;
                }
                if (new_button != nullptr) {
                    delete new_button;
                }
                if (cancel_button != nullptr) {
                    delete cancel_button;
                }
                delete actions;
                open_button = nullptr;
                return;
            }

            actions->addWidget(open_button, LEFT);
            actions->addWidget(new_button, LEFT);
            actions->addWidget(cancel_button, LEFT);
            chooser_widgets->addLayout(actions);
        } else {
            button_w = panel_w - 6;
            if (button_w < 24) {
                button_w = 24;
            }

            int content_h = (button_h * 3) + 8;
            int layout_y = panel_y + panel_h - content_h - 6;
            if (layout_y < panel_y + 18) {
                layout_y = panel_y + 18;
            }

            Layout* actions = new Layout(panel_x + 2, layout_y, button_w + 4, content_h, VERTICAL, LAYOUT_FLAG_NONE);
            if (actions == nullptr) {
                return;
            }

            open_button = new Button(button_w, button_h, open_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_OPEN;
            }));
            Button* new_button = new Button(button_w, button_h, new_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_NEW;
            }));
            Button* cancel_button = new Button(button_w, button_h, cancel_label, Function<void()>([&chooser_action]() {
                chooser_action = CHOOSER_ACTION_CANCEL;
            }));
            if (open_button == nullptr || new_button == nullptr || cancel_button == nullptr) {
                if (open_button != nullptr) {
                    delete open_button;
                }
                if (new_button != nullptr) {
                    delete new_button;
                }
                if (cancel_button != nullptr) {
                    delete cancel_button;
                }
                delete actions;
                open_button = nullptr;
                return;
            }

            actions->addWidget(open_button, LEFT);
            actions->addWidget(new_button, LEFT);
            actions->addWidget(cancel_button, LEFT);
            chooser_widgets->addLayout(actions);
        }
    };

    /**
     * Draws text from y in rows of at most max_chars, broken at spaces, or inside a word longer than a row, while a
     * row fits above max_y. @return the y after the last row drawn
     */
    auto drawWrapped = [&](int x, int y, int max_chars, int max_y, color_t color, const char* text) {
        char row[128];
        if (max_chars > (int)sizeof(row) - 1) {
            max_chars = sizeof(row) - 1;
        }

        while (*text != 0 && y + 8 <= max_y) {
            int take = strlen(text);
            if (take > max_chars) {
                /* Up to the last space that fits, when the row has one */
                int space = max_chars;
                while (space > 0 && text[space] != ' ') {
                    space--;
                }
                take = space > 0 ? space : max_chars;
            }

            memcpy(row, text, take);
            row[take] = 0;
            gfx_draw_format_text(x, y, color, "%s", row);

            text += take;
            while (*text == ' ') {
                text++;
            }
            y += 10;
        }
        return y;
    };

    auto drawChooserPanel = [&](int panel_x, int panel_y, int panel_w, int panel_h) {
        gfx_draw_rectangle(panel_x, panel_y, panel_w, panel_h, COLOR_BG);
        /* No file's text is showing, so neither are its line numbers. The gutter's line stays (#145) */
        gfx_draw_rectangle(TREE_VIEW_WIDTH, panel_y, panel_x - TEXT_GAP_WIDTH - 1 - TREE_VIEW_WIDTH, panel_h, COLOR_BG);

        /* Every row stays inside the panel, with 4 px on each side: the margin past it is painted only by reDraw */
        int row_chars = (panel_w - 8) / 8;
        if (row_chars < 1) {
            row_chars = 1;
        }
        int bottom = panel_y + panel_h;
        int y = drawWrapped(panel_x + 4, panel_y + 4, row_chars, bottom, COLOR_TEXT, "File Browser");
        y = drawWrapped(panel_x + 4, y, row_chars, bottom, COLOR_TEXT, "Pick file from tree, then Open. New File starts an empty one.");

        /* The name is cut short with "..." so the whole line fits a row */
        const char* prefix = "Selected: ";
        int prefix_len = strlen(prefix);
        char selected_line[160];
        memset(selected_line, 0, sizeof(selected_line));
        memcpy(selected_line, prefix, prefix_len);
        char* selected_label = selected_line + prefix_len;
        int label_size = sizeof(selected_line) - prefix_len;

        if (selected_file[0] == 0) {
            strncpy(selected_label, "(none)", (uint32_t)(label_size - 1));
        } else {
            int max_chars = row_chars - prefix_len;
            if (max_chars < 1) {
                max_chars = 1;
            }

            int name_len = strlen(selected_file);
            if (name_len <= max_chars) {
                strncpy(selected_label, selected_file, (uint32_t)(label_size - 1));
            } else if (max_chars > 3) {
                int keep = max_chars - 3;
                if (keep > label_size - 4) {
                    keep = label_size - 4;
                }

                strncpy(selected_label, selected_file, (uint32_t)keep);
                selected_label[keep] = '.';
                selected_label[keep + 1] = '.';
                selected_label[keep + 2] = '.';
                selected_label[keep + 3] = 0;
            } else {
                strncpy(selected_label, "...", (uint32_t)(label_size - 1));
            }
        }

        drawWrapped(panel_x + 4, y + 2, row_chars, bottom, COLOR_VGA_MEDIUM_DARK_GRAY, selected_line);
    };

    reDrawHeader();
    treeView->drawTree(this);
    reDraw(0, m_bufferHead);

    while (1) {
        int panel_x = textStartX();
        int panel_y = HEADER_HEIGHT;
        int panel_w = textClipWidth();
        int panel_h = c_height - HEADER_HEIGHT - STATUS_HEIGHT;
        if (panel_h < 28) {
            panel_h = 28;
        }

        if (chooser_widgets == nullptr ||
            panel_x != cached_panel_x || panel_y != cached_panel_y ||
            panel_w != cached_panel_w || panel_h != cached_panel_h) {
            rebuildChooserWidgets(panel_x, panel_y, panel_w, panel_h);
            cached_panel_x = panel_x;
            cached_panel_y = panel_y;
            cached_panel_w = panel_w;
            cached_panel_h = panel_h;
        }

        if (open_button != nullptr) {
            if (selected_file[0] != 0) {
                open_button->enable();
            } else {
                open_button->disable();
            }
        }

        drawChooserPanel(panel_x, panel_y, panel_w, panel_h);
        if (chooser_widgets != nullptr) {
            chooser_widgets->draw(this);
        }

        struct gfx_event event;
        gfx_get_event(&event, GFX_EVENT_BLOCKING);

        switch (event.event) {
        case GFX_EVENT_KEYBOARD:
            if (event.data == KEY_F4 || event.data == 27) {
                freeChooserWidgets();
                reDrawHeader();
                reDraw(0, m_bufferHead);
                return;
            }

            if (event.data == '\n' || event.data == 'o' || event.data == 'O') {
                chooser_action = CHOOSER_ACTION_OPEN;
            }

            if (event.data == 'n' || event.data == 'N') {
                chooser_action = CHOOSER_ACTION_NEW;
            }
            break;

        case GFX_EVENT_RESOLUTION:
            applyResolution(event.data, event.data2);
            m_keepStatus = 0;
            reDrawHeader();
            treeView->drawTree(this);
            reDraw(0, m_bufferHead);
            cached_panel_x = -1;
            cached_panel_y = -1;
            cached_panel_w = -1;
            cached_panel_h = -1;
            break;

        case GFX_EVENT_MOUSE:
            if (event.data < TREE_VIEW_WIDTH) {
                const char* file = treeView->click(event.data, event.data2);
                treeView->drawTree(this);
                if (file != nullptr) {
                    strncpy(selected_file, file, (uint32_t)(sizeof(selected_file) - 1));
                    selected_file[sizeof(selected_file) - 1] = 0;
                    drawStatusLine(COLOR_VGA_MEDIUM_DARK_GRAY, "Selected %s", selected_file);
                }
            } else {
                if (chooser_widgets != nullptr) {
                    chooser_widgets->Mouse(event.data, event.data2);
                }
            }
            break;

        case GFX_EVENT_EXIT:
            if (Quit()) {
                freeChooserWidgets();
                return;
            }
            break;

        default:
            break;
        }

        if (chooser_action == CHOOSER_ACTION_OPEN) {
            chooser_action = CHOOSER_ACTION_NONE;
            if (selected_file[0] == 0) {
                drawStatusLine(COLOR_VGA_RED, "Select a file from tree first.");
                continue;
            }

            if (!Open(selected_file)) {
                reDrawHeader();
                treeView->drawTree(this);
                reDraw(0, m_bufferHead);
                continue;
            }

            freeChooserWidgets();
            reDrawHeader();
            reDraw(0, m_bufferHead);
            return;
        }

        if (chooser_action == CHOOSER_ACTION_NEW) {
            chooser_action = CHOOSER_ACTION_NONE;

            /* An empty, untitled text, which the first save names. Unsaved changes are asked about first */
            if (!SaveMsg()) {
                continue;
            }
            if (m_fd >= 0) {
                fclose(m_fd);
                m_fd = -1;
            }
            setTitle("Editor");
            Reset();

            freeChooserWidgets();
            reDrawHeader();
            reDraw(0, m_bufferHead);
            return;
        }

        if (chooser_action == CHOOSER_ACTION_CANCEL) {
            chooser_action = CHOOSER_ACTION_NONE;
            freeChooserWidgets();
            reDrawHeader();
            reDraw(0, m_bufferHead);
            return;
        }
    }
}

/* The Help screen, a row each, with "" for an empty row */
static const char* const editor_help[] = {
    "Help",
    "F1: Save file",
    "F4: Open/create file",
    "F2/F3: Scroll up/down",
    "F9 or Esc: Close help",
    "",
    "Arrow keys: Move cursor",
    "Home/End: Start/end of line",
    "PgUp/PgDn: Move a page",
    "Shift+move: Select",
    "Ctrl+A: Select all",
    "Esc: Clear selection",
    "Ctrl+C/X/V: Copy/cut/paste",
    "Ctrl+Z/Y: Undo/redo",
    "Backspace: Delete character",
    "Delete: Delete next character",
    "Enter: New line",
    "Tab: Insert spaces",
    "Mouse: Click file in tree to open",
    "Drag over text: Select",
    "Shift+click: Select to there",
    "Double/triple click: Word/line",
};

void Editor::drawHelp()
{
    int x = textStartX();

    gfx_draw_rectangle(x, HEADER_HEIGHT, textClipWidth(), c_height - HEADER_HEIGHT - STATUS_HEIGHT, COLOR_BG);
    for (int row = 0; row < (int)(sizeof(editor_help) / sizeof(editor_help[0])); row++) {
        gfx_draw_format_text(x, HEADER_HEIGHT + (row * 8), COLOR_TEXT, "%s", editor_help[row]);
    }
}

void Editor::Help()
{
    reDrawHeader();
    treeView->drawTree(this);
    drawHelp();

    while (1) {
        struct gfx_event event;
        gfx_get_event(&event, GFX_EVENT_BLOCKING);

        switch (event.event) {
        case GFX_EVENT_KEYBOARD:
            if (event.data == 'q' || event.data == KEY_F9 || event.data == 27) {
                return;
            }
            break;

        case GFX_EVENT_MOUSE:
            if (event.data < TREE_VIEW_WIDTH) {
                treeView->click(event.data, event.data2);
                treeView->drawTree(this);
            }
            break;

        case GFX_EVENT_RESOLUTION:
            applyResolution(event.data, event.data2);
            reDrawHeader();
            treeView->drawTree(this);
            /* reDraw paints the gutter, the margin and the status line around the help. A resize leaves new space black */
            reDraw(0, m_bufferHead);
            drawHelp();
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

void Editor::EditorLoop()
{
    reDrawHeader();
    treeView->drawTree(this);
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
        case GFX_EVENT_MOUSE_UP:
            handleMouse(event);
            break;

        case GFX_EVENT_MOUSE:
            if (event.data < TREE_VIEW_WIDTH) {
                const char* file = treeView->click(event.data, event.data2);
                treeView->drawTree(this);
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
            treeView->drawTree(this);
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
    int currentCol = m_bufferEdit - currentStart;

    if (m_preferredColumn < 0) {
        m_preferredColumn = currentCol;
    }

    int currentEnd = lineEnd(currentStart);
    if (currentEnd >= m_bufferHead || m_textBuffer[currentEnd] != '\n') {
        return false;
    }

    int nextStart = currentEnd + 1;
    int nextEnd = lineEnd(nextStart);
    int nextLen = nextEnd - nextStart;

    m_bufferEdit = nextStart + minInt(m_preferredColumn, nextLen);
    return true;
}

/* Moves the cursor up a line, as cursorDown moves it down. @return false on the first line */
bool Editor::cursorUp()
{
    int currentStart = lineStart(m_bufferEdit);
    int currentCol = m_bufferEdit - currentStart;

    if (m_preferredColumn < 0) {
        m_preferredColumn = currentCol;
    }

    if (currentStart == 0) {
        return false;
    }

    int prevEnd = currentStart - 1;
    int prevStart = lineStart(prevEnd);
    int prevLen = prevEnd - prevStart;

    m_bufferEdit = prevStart + minInt(m_preferredColumn, prevLen);
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
            if (ropeDeleteRange(m_bufferEdit - 1, 2)) {
                m_bufferEdit--;
                m_dirty = 1;
                m_preferredColumn = -1;
                buffer_changed = true;
            }
            break;
        }

        if (ropeDeleteChar(m_bufferEdit - 1)) {
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

        if (ropeDeleteChar(m_bufferEdit)) {
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

    case KEY_F9:
        m_preferredColumn = -1;
        Help();
        break;

    case KEY_F4:
        m_preferredColumn = -1;
        FileChooser();
        break;

    case KEY_F1:
        m_preferredColumn = -1;
        Save();
        break;

    default:
        if (c == '\t') {
            for (int n = 0; n < 4; n++) {
                if (!ropeInsertChar(m_bufferEdit, ' ')) {
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

        if (!ropeInsertChar(m_bufferEdit, c)) {
            return;
        }

        m_bufferEdit++;
        buffer_changed = true;

        if (c == '{' && ropeInsertChar(m_bufferEdit, '}')) {
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

    if (buffer_changed) {
        ropeSyncToBuffer();
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
