#ifndef D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF
#define D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF

#include <lib/printf.h>
#include <utils/Graphics.hpp>
#include <gfx/events.h>
#include <libc.h>
#include <colors.h>
#include <fs/fs.h>

#include <utils/TreeView.hpp>
#include <utils/ScrollBar.hpp>

#define COLOR_BG COLOR_VGA_LIGHTEST_GRAY
#define COLOR_TEXT COLOR_VGA_MEDIUM_DARK_GRAY
#define COLOR_MISC COLOR_VGA_MISC
/* Behind selected text: a pale blue in the standard VGA palette */
#define COLOR_SELECTED 80

/* The tree's width when the Editor opens. Dragging its right border changes it, down to EDITOR_TREE_MIN_WIDTH */
#define TREE_VIEW_WIDTH 130
/* The narrowest the tree can be: wide enough for a few characters of each top-level name beside its scroll bar */
#define EDITOR_TREE_MIN_WIDTH 96
/* A press this far left of the tree's edge, across its gray border, or this far right, into the gutter, drags it */
#define EDITOR_TREE_GRAB_LEFT 8
#define EDITOR_TREE_GRAB_RIGHT 2
/* The grip on the tree's border that shows it can be dragged: three lines this long */
#define EDITOR_TREE_GRIP_LENGTH 12
/**
 * The tree's toggle, which collapses it to a bar at the window's left and opens it again, as Ctrl+B does: a button this
 * wide and tall, this far in from the window's left and top. It's in the same place either way, so a second click undoes
 * the first. In place of TreeView's title, it's above the tree's white box, whose left side it lines up with, and which
 * starts as far below it as it is below the top
 */
#define EDITOR_TREE_TOGGLE_X 4
#define EDITOR_TREE_TOGGLE_Y 4
#define EDITOR_TREE_TOGGLE_WIDTH 12
#define EDITOR_TREE_TOGGLE_HEIGHT 10
#define EDITOR_TREE_BOX_TOP (EDITOR_TREE_TOGGLE_Y + EDITOR_TREE_TOGGLE_HEIGHT + EDITOR_TREE_TOGGLE_Y)
/* The arrow on the toggle: columns of 1, 3, 5 and 7 pixels, like a scroll bar's arrow on its side */
#define EDITOR_TREE_ARROW_COLUMNS 4
/* The collapsed tree's bar, the whole of which takes clicks: the toggle at its top, with as much of the bar either side */
#define EDITOR_TREE_COLLAPSED_WIDTH ((2 * EDITOR_TREE_TOGGLE_X) + EDITOR_TREE_TOGGLE_WIDTH)
/**
 * The header's buttons: squares, each with an icon from /sysutil/icons, with EDITOR_BUTTON_PAD of the bar above and
 * below them. New's left side is EDITOR_BUTTON_MARGIN in from the bar's left edge, and Help's right side as far in
 * from its right edge
 */
#define EDITOR_BUTTON_SIZE 20
#define EDITOR_BUTTON_PAD 2
#define EDITOR_BUTTON_MARGIN 5
#define EDITOR_BUTTON_GAP 2
/* Below the bar's top edge, the face and its light line */
#define EDITOR_BUTTON_Y (2 + EDITOR_BUTTON_PAD)

/* The bar above the text, with the New, Save and Help buttons: the buttons and their padding, between its edges */
#define HEADER_HEIGHT (EDITOR_BUTTON_Y + EDITOR_BUTTON_SIZE + EDITOR_BUTTON_PAD + 3)
/**
 * The status line below the text: a gray band the pane's width, under the line numbers too, with a sunken field from
 * the gutter's line that the status text sits in, lined up with the text above. The band shows EDITOR_STATUS_MARGIN
 * above and below the field, which has a pixel of edge and one of space around the text's 8 rows.
 */
#define EDITOR_STATUS_MARGIN 2
#define STATUS_HEIGHT ((2 * EDITOR_STATUS_MARGIN) + 2 + 2 + 8)

/* The header bar and its buttons are drawn in lib/graphics.h's COLOR_BAR_FACE, COLOR_BAR_LIGHT and COLOR_BAR_SHADOW */

/**
 * Around the text: between it and the header above, the gutter's line at its left and the status line below. At its
 * right, the text's scroll bar is at least this far away, and further by what's left over from whole columns
 */
#define TEXT_PADDING 5
/* The first row of text */
#define TEXT_TOP (HEADER_HEIGHT + TEXT_PADDING)
/* A row of text: a character's 8 pixels, with the space between rows split above and below them */
#define TEXT_ROW_HEIGHT 10
#define TEXT_CHAR_Y ((TEXT_ROW_HEIGHT - 8) / 2)
/* A tab is drawn as space to the next multiple of this many columns, and the Tab key inserts this many spaces (#135) */
#define EDITOR_TAB_WIDTH 4

/**
 * The most the text's buffer grows to: the text and the 0 after it, so 262,143 characters (#213). A bigger file opens
 * read-only. The buffer starts at EDITOR_BUFFER_MIN, or as much as the file opened needs, and doubles as edits need
 */
#define EDITOR_BUFFER_SIZE (256 * 1024)
#define EDITOR_BUFFER_MIN (16 * 1024)
#define EDITOR_DEFAULT_TEXT_WIDTH 400
#define EDITOR_DEFAULT_GUTTER_WIDTH 32
#define EDITOR_DEFAULT_RIGHT_PANE_WIDTH (EDITOR_DEFAULT_TEXT_WIDTH + EDITOR_DEFAULT_GUTTER_WIDTH + (2 * TEXT_PADDING) + SCROLLBAR_WIDTH)
#define EDITOR_DEFAULT_WINDOW_WIDTH (TREE_VIEW_WIDTH + EDITOR_DEFAULT_RIGHT_PANE_WIDTH)
/* 27 rows of text */
#define EDITOR_DEFAULT_HEIGHT 326

#define EDITOR_MIN_RIGHT_PANE_WIDTH 48
#define EDITOR_MIN_HEIGHT (HEADER_HEIGHT + STATUS_HEIGHT + (2 * TEXT_PADDING) + TEXT_ROW_HEIGHT)
#define EDITOR_MAX_VIEW_ROWS 1024
/* How often a drag held above or below the text scrolls it a row */
#define EDITOR_DRAG_SCROLL_MS 50
/* How many of the } that typing { adds are kept track of, one per block still open; past that, the oldest is dropped */
#define EDITOR_AUTO_BRACES_MAX 32
/**
 * The undo history's first block, and the most it grows to, doubling, before the oldest steps are dropped. It holds a
 * step of twice the text, a full paste over a full text, with as much again for the steps before it: a step bigger
 * than the history turns it off for the rest of the text (undoReserve)
 */
#define EDITOR_UNDO_MIN (16 * 1024)
#define EDITOR_UNDO_MAX (4 * EDITOR_BUFFER_SIZE)

/* What kind of key made an undo step: typing, Backspace or Delete join the step the last key of the same kind made */
enum {
    EDITOR_UNDO_CLOSED = 0,
    EDITOR_UNDO_TYPING,
    EDITOR_UNDO_BACKSPACE,
    EDITOR_UNDO_DELETE,
    /* Enter, Tab, cut and paste, a step each */
    EDITOR_UNDO_ALONE
};

/**
 * A prompt on the status line, which takes every key until it's closed: Ctrl+G's line number and Ctrl+F's Find (#137).
 * What's typed at it holds up to EDITOR_PROMPT_MAX characters, and a line number up to EDITOR_LINE_DIGITS, since the
 * text, at most 262,143 characters (EDITOR_BUFFER_SIZE less the 0 after it), has at most 262,144 lines
 */
enum {
    EDITOR_PROMPT_NONE = 0,
    EDITOR_PROMPT_LINE,
    EDITOR_PROMPT_FIND
};
#define EDITOR_PROMPT_MAX 64
#define EDITOR_LINE_DIGITS 6

/* The header's buttons: New and Save from the left, and Help at the right */
enum {
    EDITOR_BUTTON_NONE = -1,
    EDITOR_BUTTON_NEW,
    EDITOR_BUTTON_SAVE,
    EDITOR_BUTTON_HELP,
    EDITOR_BUTTON_COUNT
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
    bool SaveAs();
    void Help();
    bool Open(char* path);
    void putChar(unsigned char c, int modifiers = 0);
    void Lex();
    bool Quit();
    bool SaveMsg();
    void setFd(int fd);

    void EditorLoop();
    void New();
    void setColor(color_t color);
    void Reset();
    void reDrawHeader();

    void scroll(int lines);
    int countLines() const;

private:
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

    int indexAtPoint(int x, int y, bool under = false) const;
    void handleMouse(const struct gfx_event& event);
    bool buttonPosition(int button, int* x) const;
    int buttonAt(int x, int y) const;
    void drawButton(int button, bool pressed);
    bool buttonMouse(const struct gfx_event& event, int* clicked);
    void drawTreePane();
    void drawTextScrollBar(int cols, int rows, int view_start);
    void scrollToRow(int row);
    int clampTreeWidth(int width) const;
    void setTreeWidth(int width);
    bool treeResizeMouse(const struct gfx_event& event);
    void setTreeCollapsed(bool collapsed);
    bool treeToggleAt(int x, int y) const;
    void drawTreeToggle(bool pressed);
    bool treeToggleMouse(const struct gfx_event& event, bool* clicked);
    void selectAround(int index, bool line);
    void dragScrollTick();

    int visibleLines() const;
    int visibleColumns() const;
    int countNewlines(int from, int to) const;
    int lineNumberAtIndex(int index) const;
    int indexFromLine(int line) const;
    void resetLines();
    int lineStart(int index) const;
    int lineEnd(int index) const;
    int lineRowsFromStart(int line_start, int cols) const;
    int lineColumn(int index) const;
    int indexAtColumn(int line_start, int column) const;

    int gutterWidth() const;
    int textStartX() const;
    int textClipWidth() const;

    int viewStartIndex(int cols) const;
    int nextVisualRowStart(int row_start_index, int cols) const;
    int prevVisualRowStart(int row_start_index, int cols) const;
    int rowsFrom(int index, int cols) const;
    int rowsOfLine(int line_start, int cols) const;
    int nextLineStart(int line_start) const;
    int rowsOfLines(int from, int to, int cols) const;
    void countRows(int cols) const;
    int textRows(int cols) const;
    int rowsBeforeLine(int line_start, int cols) const;
    int rowsAbove(int index, int cols) const;
    int rowsBelow(int index, int cols) const;
    void cursorFromViewStart(int view_start, int cols, int* out_row, int* out_col) const;
    void placeFromViewStart(int index, int view_start, int cols, int* out_row, int* out_col) const;

    void setScrollFromIndex(int index, int cols);
    void normalizeScroll(int cols);
    void ensureCursorVisible(int cols, int rows);

    void getCursorLineCol(int& line, int& col) const;
    bool cursorDown();
    bool cursorUp();
    bool hasUnsavedChanges() const;

    void drawLineNumbers(const int* row_lines, int row_count);
    void drawStatus();
    void drawStatusLine(color_t color, const char* fmt, ...);
    void openPrompt(int kind);
    void closePrompt();
    void drawPrompt();
    void promptKey(unsigned char c, int modifiers);
    bool matchAt(int index, int len) const;
    void findNext(bool backward);
    void goToLine(int line);
    void revealRange(int start, int end);
    void applyResolution(int window_width, int window_height);
    bool showNewFileDialog(char* path, int path_size);

    void clearHistory();
    bool reserveText(int length);
    void shrinkText();
    bool bufferInsertChar(int index, unsigned char c);
    bool bufferInsertText(int index, const unsigned char* data, int len);
    bool bufferDeleteChar(int index);
    bool bufferDeleteRange(int index, int count);

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
    /**
     * The text's scroll bar, along the window's right edge from the header to the status line. Its strip is kept
     * whether or not it's shown, so the text doesn't rewrap when it comes and goes
     */
    ScrollBar m_textBar;
    /**
     * The tree's width, or while it's collapsed, its bar's, EDITOR_TREE_COLLAPSED_WIDTH, and the window's, which the
     * tree and the pane right of it share. m_treeOpenWidth is the width the tree had when it was collapsed, which it
     * gets back when it's opened, if the window still has room
     */
    int m_treeWidth;
    int m_windowWidth;
    bool m_treeCollapsed;
    int m_treeOpenWidth;
    /* Set from a press on the tree's border until its release. m_treeGrabX is where the press was, from the border */
    bool m_resizingTree;
    int m_treeGrabX;
    /* Set from a press on the tree's toggle until its release. It's drawn pressed while the mouse is over it, m_toggleShown */
    bool m_togglePressed;
    bool m_toggleShown;
    /**
     * Set from any press until its release. Ctrl+B does nothing meanwhile, so a drag on the tree, its border or its
     * scroll bar isn't left half done, with the tree's scroll bar thinking it's still pressed
     */
    bool m_mouseHeld;
    /**
     * The last press was on the tree, not its border. A click comes after every release, where the mouse is, and the
     * tree picks its row by height alone, so it takes only clicks whose press was on it too: a drag from the text, a
     * button or the border, released over it, would open a file
     */
    bool m_pressOnTree;
    /* The header buttons' icons, by EDITOR_BUTTON_* */
    struct image m_buttonIcons[EDITOR_BUTTON_COUNT];
    /* The button a press started on, until its release; EDITOR_BUTTON_NONE for none. It's drawn pressed while the mouse is over it */
    int m_pressedButton;
    bool m_pressedShown;

    int m_fd;
    /**
     * The text: its m_bufferHead characters, then a 0, in m_bufferSize bytes, which grow as edits need, up to
     * EDITOR_BUFFER_SIZE (reserveText). Edited only by the buffer* functions
     */
    unsigned char* m_textBuffer;
    /* The text's lines, one more than its newlines, which the buffer* functions keep as they change it */
    int m_lineCount;
    /**
     * The line anchor: line m_anchorLine, counted from 0, starts at m_anchorIndex. lineNumberAtIndex and indexFromLine
     * count from it rather than from the text's start, and move it to where they end up; the buffer* functions keep
     * it right as they change the text (#213)
     */
    mutable int m_anchorLine;
    mutable int m_anchorIndex;
    /**
     * The text's rows at m_rowsCols columns, for the scroll bar and paging, or none counted while m_rowsCols is 0:
     * m_totalRows in all, and the row anchor, the line starting at m_rowAnchorIndex with m_rowAnchorRows rows above it.
     * countRows counts them for a new width; the buffer* functions keep them right as they change the text (#213)
     */
    mutable int m_rowsCols;
    mutable int m_totalRows;
    mutable int m_rowAnchorIndex;
    mutable int m_rowAnchorRows;
    int m_bufferSize;
    int m_fileSize;
    int m_bufferHead;
    int m_bufferEdit;
    /* An edit was made since the last save or open. Unsaved changes are the undo history's to tell, while there is one */
    int m_dirty;
    /* The file is bigger than the buffer, which holds only its start: a save would cut it short, so it can't be edited */
    bool m_readOnly;
    /* Where the } are that typing { added and typing } steps over, oldest first. Every insert and delete moves them (#119) */
    int m_autoBraces[EDITOR_AUTO_BRACES_MAX];
    int m_autoBraceCount;
    /**
     * The undo history (#161): the steps done, oldest first, then those undone, which redo does again. The four buffer*
     * functions that edit the text record into it. Allocated at the first edit, m_undoSize bytes, and freed with the text
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
    /* The column Up and Down keep, as lineColumn counts it, while they're pressed one after another; -1 for none */
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
    /* The prompt on the status line, EDITOR_PROMPT_NONE for none, and what's typed at it: m_promptLen characters and a 0 */
    int m_prompt;
    char m_promptText[EDITOR_PROMPT_MAX + 1];
    int m_promptLen;
    /* What's at the prompt was filled in, not typed, so the first key typed replaces it */
    bool m_promptFilled;
    /* The last text searched for, which Ctrl+F fills in without a selection */
    char m_findText[EDITOR_PROMPT_MAX + 1];
    /* After a search, which match is selected, from 1, of how many; m_findCount is 0 for none, -1 until a search */
    int m_findIndex;
    int m_findCount;

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
};

#endif /* D2830F9E_59AE_4E2A_B9AB_EB0EC93872EF */
