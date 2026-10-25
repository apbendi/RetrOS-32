#ifndef D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF
#define D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF

#include <lib/printf.h>
#include <utils/Graphics.hpp>
#include <gfx/events.h>
#include <libc.h>
#include <colors.h>
#include <fs/fs.h>

#include <utils/TreeView.hpp>

#define COLOR_BG COLOR_VGA_LIGHTEST_GRAY
#define COLOR_TEXT COLOR_VGA_MEDIUM_DARK_GRAY
#define COLOR_MISC COLOR_VGA_MISC
/* Behind selected text: a pale blue in the standard VGA palette */
#define COLOR_SELECTED 80

#define TREE_VIEW_WIDTH 130
#define HEADER_HEIGHT 13
#define STATUS_HEIGHT 10

#define TEXT_GAP_WIDTH 1
#define TEXT_RIGHT_PADDING 4

#define EDITOR_BUFFER_SIZE (64 * 1024)
#define ROPE_CHUNK_SIZE 256
#define EDITOR_DEFAULT_TEXT_WIDTH 400
#define EDITOR_DEFAULT_GUTTER_WIDTH 32
#define EDITOR_DEFAULT_RIGHT_PANE_WIDTH (EDITOR_DEFAULT_TEXT_WIDTH + EDITOR_DEFAULT_GUTTER_WIDTH + TEXT_GAP_WIDTH + TEXT_RIGHT_PADDING)
#define EDITOR_DEFAULT_WINDOW_WIDTH (TREE_VIEW_WIDTH + EDITOR_DEFAULT_RIGHT_PANE_WIDTH)
#define EDITOR_DEFAULT_HEIGHT 320

#define EDITOR_MIN_RIGHT_PANE_WIDTH 48
#define EDITOR_MIN_HEIGHT (HEADER_HEIGHT + STATUS_HEIGHT + 8)
#define EDITOR_MAX_VIEW_ROWS 1024
/* How often a drag held above or below the text scrolls it a row */
#define EDITOR_DRAG_SCROLL_MS 50

struct keyword {
    char word[10];
    color_t color;
};

class Editor : public Window {
public:
    Editor();
    ~Editor();

    bool Save();
    void Help();
    bool Open(char* path);
    void putChar(unsigned char c, int modifiers = 0);
    void Lex();
    bool Quit();
    bool SaveMsg();
    void setFd(int fd);

    void drawChar(unsigned char c, color_t bg);
    void EditorLoop();
    void FileChooser();
    void setColor(color_t color);
    void Reset();
    void reDrawHeader();

    void scroll(int lines);
    int countLines() const;

private:
    struct rope_node {
        struct rope_node* prev;
        struct rope_node* next;
        int len;
        unsigned char data[ROPE_CHUNK_SIZE];
    };

    void reDraw(int from, int to);
    void drawSelection(int view_start, int cols, int rows, int text_x);
    void highlightSyntax(unsigned char* start);

    bool hasSelection() const;
    bool selectionRange(int& start, int& end) const;
    void clearSelection();
    bool deleteSelection();
    bool deleteRange(int start, int end);
    bool insertText(int index, const unsigned char* data, int len);
    bool copySelection();
    void paste();

    int indexAtPoint(int x, int y) const;
    void handleMouse(const struct gfx_event& event);
    void selectAround(int index, bool line);
    void dragScrollTick();

    int visibleLines() const;
    int visibleColumns() const;
    int lineNumberAtIndex(int index) const;
    int indexFromLine(int line) const;
    int lineStart(int index) const;
    int lineEnd(int index) const;
    int lineRowsFromStart(int line_start, int cols) const;

    int gutterWidth() const;
    int textStartX() const;
    int textClipWidth() const;

    int viewStartIndex(int cols) const;
    int nextVisualRowStart(int row_start_index, int cols) const;
    int prevVisualRowStart(int row_start_index, int cols) const;
    int rowsFrom(int index, int cols) const;
    void cursorFromViewStart(int view_start, int cols, int* out_row, int* out_col) const;

    void setScrollFromIndex(int index, int cols);
    void normalizeScroll(int cols);
    void ensureCursorVisible(int cols, int rows);

    void getCursorLineCol(int& line, int& col) const;
    bool cursorDown();
    bool cursorUp();
    void drawHelp();
    bool hasUnsavedChanges() const;

    void drawLineNumbers(const int* row_lines, int row_count);
    void drawStatusLine(color_t color, const char* fmt, ...);
    void applyResolution(int window_width, int window_height);
    bool showNewFileDialog(char* out_path, int out_path_size);

    void ropeClear();
    bool ropeLoadFromBuffer(const unsigned char* data, int len);
    bool ropeSyncToBuffer();
    bool ropeInsertChar(int index, unsigned char c);
    bool ropeDeleteChar(int index);
    bool ropeDeleteRange(int index, int count);
    bool ropeInsertText(int index, const unsigned char* data, int len);
    bool ropeLocate(int index, struct rope_node** out_node, int* out_offset) const;
    struct rope_node* ropeCreateNode();
    void ropeDetachNode(struct rope_node* node);
    void ropeTryMerge(struct rope_node* node);

    TreeView* treeView;

    int m_fd;
    unsigned char* m_textBuffer;
    int m_bufferSize;
    int m_fileSize;
    int m_bufferHead;
    int m_bufferEdit;
    int m_x;
    int m_y;
    int m_dirty;
    /* The file is bigger than the buffer, which holds only its start: a save would cut it short, so it can't be edited */
    bool m_readOnly;
    int m_preferredColumn;
    /* Where the selection started, which runs from here to the cursor; -1 for none */
    int m_selAnchor;
    /* Set from a press in the text until its release. m_dragScroll is -1 or 1 while the mouse is above or below the text, at m_dragX */
    bool m_dragging;
    int m_dragScroll;
    int m_dragX;
    char m_currentPath[FS_PATH_MAX];
    /* A message, like an error, is on the status line, which redraws leave in place of the file's name until the next event */
    int m_keepStatus;

    int scrollY;
    int m_scrollWrap;

    color_t m_textColor;

#define KEYWORD_TYPE COLOR_VGA_LIGHT_BLUE
#define KEYWORD_SYS COLOR_VGA_PURPLE
#define KEYWORD_BRANCH COLOR_VGA_RED
#define KEYWORD_FUNC COLOR_VGA_MEDIUM_DARK_GRAY
    struct keyword keyWords[20] = {
        {"char", KEYWORD_TYPE},
        {"else", KEYWORD_BRANCH}, {"enum", KEYWORD_TYPE}, {"if", KEYWORD_BRANCH},
        {"int", KEYWORD_TYPE}, {"return", KEYWORD_SYS}, {"sizeof", KEYWORD_FUNC},
        {"while", KEYWORD_BRANCH}, {"open", KEYWORD_FUNC}, {"printf", KEYWORD_FUNC},
        {"malloc", KEYWORD_FUNC}, {"main", KEYWORD_FUNC}, {"void", KEYWORD_TYPE}
    };

    int c_width;
    int c_height;

    struct rope_node* m_ropeHead;
    struct rope_node* m_ropeTail;
};

#endif /* D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF */
