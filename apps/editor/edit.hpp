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
/* How many of the } that typing { adds are kept track of, one per block still open; past that, the oldest is dropped */
#define EDITOR_AUTO_BRACES_MAX 32
/**
 * The undo history's first block, and the most it grows to, doubling, before the oldest steps are dropped. One step can
 * hold about 128 KB, a 64 KB paste over a 64 KB text
 */
#define EDITOR_UNDO_MIN (16 * 1024)
#define EDITOR_UNDO_MAX (256 * 1024)

/* What kind of key made an undo step: typing, Backspace or Delete join the step the last key of the same kind made */
enum {
    EDITOR_UNDO_CLOSED = 0,
    EDITOR_UNDO_TYPING,
    EDITOR_UNDO_BACKSPACE,
    EDITOR_UNDO_DELETE,
    /* Enter, Tab, cut and paste, a step each */
    EDITOR_UNDO_ALONE
};

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

    /**
     * One undo step: del characters at pos became ins characters. The step is followed in the history by the del
     * characters, then the ins characters, then padding to a multiple of 4, so the next step's ints are aligned
     */
    struct undo_step {
        /* How many bytes the step before it takes, to find that one; 0 for the oldest */
        int prev;
        int pos;
        int del;
        int ins;
        /* The cursor and selection anchor (-1 for none) before the step, and the cursor after it */
        int cursorBefore;
        int anchorBefore;
        int cursorAfter;
    };

    void handleKey(unsigned char c, int modifiers);
    void reDraw(int from, int to);
    void drawSelection(int view_start, int cols, int rows, int text_x);
    void highlightSyntax(unsigned char* start);
    void highlightWordAt(int index);

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
    bool ropeAddChar(int index, unsigned char c);
    bool ropeDeleteChar(int index);
    bool ropeDeleteRange(int index, int count);
    bool ropeInsertText(int index, const unsigned char* data, int len);
    bool ropeLocate(int index, struct rope_node** out_node, int* out_offset) const;
    void ropeCopy(int index, int count, unsigned char* out) const;
    struct rope_node* ropeCreateNode();
    void ropeDetachNode(struct rope_node* node);
    void ropeTryMerge(struct rope_node* node);

    void autoBracesInserted(int index, int len);
    void autoBracesDeleted(int index, int count);
    int autoBraceAt(int index) const;

    void undo();
    void redo();
    bool undoApply(const struct undo_step* step, bool redo);
    void undoKeyStart(int kind);
    void undoKeyEnd();
    void undoInserted(int index, const unsigned char* data, int len);
    void undoDeleting(int index, int len);
    struct undo_step* undoStepFor(int index, int len, bool inserted);
    bool undoReserve(int need, int keep);
    static int undoStepBytes(int text);
    void undoClear();
    void undoFail();

    TreeView* treeView;

    int m_fd;
    unsigned char* m_textBuffer;
    int m_bufferSize;
    int m_fileSize;
    int m_bufferHead;
    int m_bufferEdit;
    int m_x;
    int m_y;
    /* An edit was made since the last save or open. Unsaved changes are the undo history's to tell, while there is one */
    int m_dirty;
    /* The file is bigger than the buffer, which holds only its start: a save would cut it short, so it can't be edited */
    bool m_readOnly;
    /* Where the } are that typing { added and typing } steps over, oldest first. Every insert and delete moves them (#119) */
    int m_autoBraces[EDITOR_AUTO_BRACES_MAX];
    int m_autoBraceCount;
    /**
     * The undo history (#161): the steps done, oldest first, then those undone, which redo does again. The four rope
     * functions that edit record into it. Allocated at the first edit, m_undoSize bytes, and freed with the text
     */
    unsigned char* m_undo;
    int m_undoSize;
    /* The steps take m_undoUsed bytes, of which the first m_undoAt are those done. The last done starts at m_undoLast, -1 for none */
    int m_undoUsed;
    int m_undoAt;
    int m_undoLast;
    /* How many steps are done, counting the oldest ones dropped, m_undoDropped. m_undoSaved is how many were at the last save, -1 if they can't be again */
    int m_undoDone;
    int m_undoDropped;
    int m_undoSaved;
    /* The last step takes the edits of the key being handled, and of the next key if it's this kind too; EDITOR_UNDO_CLOSED for none */
    int m_undoJoin;
    /* The kind of key being handled, and the cursor and selection anchor (-1 for none) before it, for a step it starts */
    int m_undoKey;
    int m_undoCursor;
    int m_undoAnchor;
    /* Undo or redo is changing the text, which isn't recorded */
    bool m_undoing;
    /* The history ran out of memory, so edits go unrecorded until the text is replaced */
    bool m_undoFailed;
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
