/**
 * @file TreeView.hpp
 * @author Joe Bayer (joexbayer)
 * @brief A tree view widget
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef __TREEVIEW_HPP__
#define __TREEVIEW_HPP__

#include <utils/StringHelper.hpp>
#include <utils/Graphics.hpp>
#include <utils/cppUtils.hpp>
#include <lib/syscall.h>
#include <utils/Function.hpp>
#include <utils/ScrollBar.hpp>
#include <libc.h>

#include <fs/fs.h>

/* Behind the highlighted file's row: the pale blue the Editor draws behind selected text */
#define TREE_VIEW_HIGHLIGHT 80

/**
 * @brief A tree view widget
 * @details This class implements a tree view widget that can be used to
 * display a file system tree. It is used by the Finder application
 * to display the file system tree.
 * 
 * @warning Assumes that a window is present.
 */
class TreeView {
public:
    TreeView(int x, int y, int width, int height)
        : x(x), y(y), width(width), height(height) {
        /* Initialize any other required attributes */
        path = new String("/");
        memset(&root, 0, sizeof(root));
        root.name = path;
        root.isDirectory = 1;
        root.isExpanded = 1;
        root.childrenCount = 0;
        root.childrenCapacity = 16;
        root.children = new TreeNode[root.childrenCapacity];
        if (root.children != nullptr) {
            memset(root.children, 0, root.childrenCapacity * sizeof(TreeNode));
            loadDirectory(&root);
        }

        /* From /sysutil/icons, shared with Finder and the kernel. A file that can't be read gives a placeholder */
        gfx_load_icon("folder_16", &folderIcon);
        gfx_load_icon("file_16", &fileIcon);

        scrollable = false;
    }

    ~TreeView() {
        gfx_free_icon(&folderIcon);
        gfx_free_icon(&fileIcon);
        clearChildren(&root);
        if (root.children != nullptr) {
            delete[] root.children;
            root.children = nullptr;
        }
        root.childrenCapacity = 0;
        delete path;
    }

    /* highlight is a file's full path, whose row is drawn highlighted when it shows, or nullptr for none */
    void drawTree(Window* window, const char* highlight = nullptr) {
        placeScrollBar();

        /* Draw the tree view background */
        window->drawContouredRect(x, y, width, height);
        window->drawContouredBox(8, 12, width - 16, height-20, COLOR_WHITE);

        window->drawFormatText(2, 2, COLOR_BLACK, "Tree View");
        /* Rows count from the first shown, so those scrolled above it are negative */
        int row = -firstRow();
        int max_rows = visibleRows();
        drawTreeRecursive(&root, 1, &row, -1, max_rows, highlight);

        if (scrollBarShown()) {
            scrollBar.draw();
        }
    }

    /**
     * Gives the tree a scroll bar along its box's right side, shown while its rows don't all fit. Its owner passes it
     * presses, moves and releases with mouse(). Without one, rows past the box's bottom aren't shown.
     */
    void setScrollable(bool on) {
        scrollable = on;
    }

    /**
     * Gives the scroll bar a press, or a move or release after one. It redraws itself when only its look changes.
     * @return 0 for an event that isn't the bar's, or SCROLLBAR_TAKEN, with SCROLLBAR_SCROLLED when the rows moved, and
     * the tree needs drawing again
     */
    int mouse(const struct gfx_event& event) {
        if (!scrollable) {
            return 0;
        }

        int result = scrollBar.mouse(event);
        if ((result & SCROLLBAR_REDRAW) && !(result & SCROLLBAR_SCROLLED)) {
            scrollBar.draw();
        }
        return result;
    }

    /**
     * Expands the directories that hold path, a file's full path, so its row shows, if the tree has the file. With a
     * scroll bar, a row out of view is scrolled to the middle; without one, a row past the box's bottom still doesn't
     * show.
     * @return whether the tree has it
     */
    bool reveal(const char* path) {
        if (path == nullptr || path[0] == 0) {
            return false;
        }
        if (!revealIn(&root, path)) {
            return false;
        }

        if (scrollable) {
            int row = 0;
            int target = rowOf(&root, path, &row);
            int visible = visibleRows();
            int first = scrollBar.first();
            if (target >= 0 && (target < first || target >= first + visible)) {
                first = target - (visible / 2);
            }
            scrollBar.set(countRows(&root), visible, first);
        }
        return true;
    }

    void resize(int newWidth, int newHeight) {
        width = newWidth;
        height = newHeight;
    }

    void refresh() {
        clearChildren(&root);
        if (root.children == nullptr) {
            root.childrenCapacity = 16;
            root.children = new TreeNode[root.childrenCapacity];
            if (root.children == nullptr) {
                root.childrenCapacity = 0;
                return;
            }
        }
        memset(root.children, 0, root.childrenCapacity * sizeof(TreeNode));
        root.childrenCount = 0;
        loadDirectory(&root);
    }

    const char* click(int x, int y) {
        int max_rows = visibleRows();
        if (max_rows <= 0) {
            return nullptr;
        }

        if (y < 16 || y >= (height - 8)) {
            return nullptr;
        }

        /* The scroll bar's strip isn't a row's */
        if (scrollBarShown() && x >= scrollBar.x()) {
            return nullptr;
        }

        int shown_row = (y / 16) - 1;
        if (shown_row < 0 || shown_row >= max_rows) {
            return nullptr;
        }

        int target_row = shown_row + firstRow();
        int row = 0;
        struct TreeNode* hit = findNodeByRow(&root, &row, target_row);
        if (hit == nullptr) {
            return nullptr;
        }

        if (hit->isDirectory) {
            if (!hit->isExpanded && !hit->isLoaded) {
                loadDirectory(hit);
            }
            hit->isExpanded = !hit->isExpanded;
            return nullptr;
        }

        return hit->name->getData();
    }

    void update(const char* newPath) {
        /* Update the tree view with the new path */
        path->concat(newPath);
        refresh();
    }

private:
    /* Logic of caching and interaction */
    struct TreeNode {
        String* name;
        int isDirectory;
        int isExpanded;
        /* Whether a directory's entries have been read, which is when it's first expanded or revealed */
        int isLoaded;
        int childrenCount;
        int childrenCapacity;
        struct TreeNode* children;
    } root;
    int x, y, width, height;
    String* path;
    struct image folderIcon;
    struct image fileIcon;
    /* Whether the tree has a scroll bar; its first is the first row shown */
    bool scrollable;
    ScrollBar scrollBar;

    int firstRow() const
    {
        return scrollable ? scrollBar.first() : 0;
    }

    bool scrollBarShown() const
    {
        return scrollable && scrollBar.shown();
    }

    /* Inside the box's right border, the box's height inside its top and bottom ones, with the rows as they are now */
    void placeScrollBar()
    {
        if (!scrollable) {
            return;
        }
        scrollBar.place(width - 9 - SCROLLBAR_WIDTH, 13, height - 22);
        scrollBar.set(countRows(&root), visibleRows(), scrollBar.first());
    }

    /* Where rows end, at the scroll bar when it's shown, or else the box's right border: the first x past them */
    int rowsRight() const
    {
        return scrollBarShown() ? scrollBar.x() : width - 9;
    }

    /* The rows node's children take, with those of directories expanded */
    int countRows(struct TreeNode* node)
    {
        int rows = 0;
        for (int i = 0; i < node->childrenCount; i++) {
            rows++;
            if (node->children[i].isDirectory && node->children[i].isExpanded) {
                rows += countRows(&node->children[i]);
            }
        }
        return rows;
    }

    /* The row of the file at path, counting from *row, as findNodeByRow does, or -1 if it isn't showing */
    int rowOf(struct TreeNode* node, const char* path, int* row)
    {
        for (int i = 0; i < node->childrenCount; i++) {
            struct TreeNode* child = &node->children[i];
            if (!child->isDirectory && pathMatches(path, child->name->getData(), false)) {
                return *row;
            }

            (*row)++;
            if (child->isDirectory && child->isExpanded) {
                int found = rowOf(child, path, row);
                if (found >= 0) {
                    return found;
                }
            }
        }
        return -1;
    }

    struct TreeNode* findNodeByRow(struct TreeNode* node, int* row, int target_row)
    {
        for (int i = 0; i < node->childrenCount; i++) {
            if (*row == target_row) {
                return &node->children[i];
            }

            (*row)++;
            if (node->children[i].isDirectory && node->children[i].isExpanded) {
                struct TreeNode* found = findNodeByRow(&node->children[i], row, target_row);
                if (found != nullptr) {
                    return found;
                }
            }
        }
        return nullptr;
    }

    /**
     * Whether path is name, or with prefix set, starts with it, ignoring case as FAT does. A node's name is its full
     * path, a directory's with a "/" after it, so a path that starts with a directory's name is inside it.
     */
    static bool pathMatches(const char* path, const char* name, bool prefix)
    {
        int i = 0;
        for (; name[i] != 0; i++) {
            if (tolower(path[i]) != tolower(name[i])) {
                return false;
            }
        }
        return prefix || path[i] == 0;
    }

    bool revealIn(struct TreeNode* node, const char* path)
    {
        for (int i = 0; i < node->childrenCount; i++) {
            struct TreeNode* child = &node->children[i];
            const char* name = child->name->getData();

            if (!child->isDirectory) {
                if (pathMatches(path, name, false)) {
                    return true;
                }
            } else if (pathMatches(path, name, true)) {
                /* Only the directories on the path are read */
                if (!child->isLoaded) {
                    loadDirectory(child);
                }
                if (revealIn(child, path)) {
                    /* Only once the file is found, so a path the tree doesn't have expands nothing */
                    child->isExpanded = 1;
                    return true;
                }
            }
        }
        return false;
    }

    int visibleRows() const
    {
        int usable_height = height - 24;
        if (usable_height < 16) {
            return 0;
        }

        int rows = usable_height / 16;
        if (rows < 1) {
            rows = 1;
        }

        return rows;
    }

    void drawClippedText(int x, int y, const char* text)
    {
        if (text == nullptr) {
            return;
        }

        /* Short of where rows end */
        int right_limit = rowsRight() - 3;
        int pixels = right_limit - x;
        if (pixels < 8) {
            return;
        }

        int max_chars = pixels / 8;
        if (max_chars <= 0) {
            return;
        }

        int len = strlen(text);
        if (len <= max_chars) {
            gfx_draw_format_text(x, y, COLOR_BLACK, "%s", text);
            return;
        }

        char clipped[64];
        int keep = max_chars;
        if (keep > 63) {
            keep = 63;
        }

        if (keep >= 4) {
            keep -= 3;
            memcpy(clipped, text, keep);
            clipped[keep + 0] = '.';
            clipped[keep + 1] = '.';
            clipped[keep + 2] = '.';
            clipped[keep + 3] = 0;
        } else {
            for (int i = 0; i < keep; i++) {
                clipped[i] = '.';
            }
            clipped[keep] = 0;
        }

        gfx_draw_format_text(x, y, COLOR_BLACK, "%s", clipped);
    }

    void drawIcon(int x, int y, const struct image* icon) {
        gfx_draw_image(x, y, icon);
    }

    void clearNodeRecursive(struct TreeNode* node)
    {
        if (node == nullptr) {
            return;
        }

        clearChildren(node);

        if (node->children != nullptr) {
            delete[] node->children;
            node->children = nullptr;
        }
        node->childrenCapacity = 0;
        node->childrenCount = 0;

        if (node->name != nullptr) {
            delete node->name;
            node->name = nullptr;
        }
    }

    void clearChildren(struct TreeNode* node)
    {
        if (node == nullptr || node->children == nullptr) {
            if (node != nullptr) {
                node->childrenCount = 0;
            }
            return;
        }

        for (int i = 0; i < node->childrenCount; i++) {
            clearNodeRecursive(&node->children[i]);
        }

        memset(node->children, 0, node->childrenCapacity * sizeof(TreeNode));
        node->childrenCount = 0;
    }

    bool ensureChildCapacity(struct TreeNode* parent)
    {
        if (parent == nullptr) {
            return false;
        }

        if (parent->children == nullptr) {
            parent->childrenCapacity = 16;
            parent->children = new TreeNode[parent->childrenCapacity];
            if (parent->children == nullptr) {
                parent->childrenCapacity = 0;
                return false;
            }
            memset(parent->children, 0, parent->childrenCapacity * sizeof(TreeNode));
        }

        if (parent->childrenCount < parent->childrenCapacity) {
            return true;
        }

        int new_capacity = parent->childrenCapacity * 2;
        if (new_capacity < 16) {
            new_capacity = 16;
        }

        TreeNode* resized = new TreeNode[new_capacity];
        if (resized == nullptr) {
            return false;
        }

        memset(resized, 0, new_capacity * sizeof(TreeNode));
        memcpy(resized, parent->children, parent->childrenCount * sizeof(TreeNode));
        delete[] parent->children;

        parent->children = resized;
        parent->childrenCapacity = new_capacity;
        return true;
    }

    /* Collapsed, and a directory's children are neither read nor allocated until it's expanded or revealed */
    struct TreeNode* insertNode(struct TreeNode* parent, const char* name, int isDirectory) {
        if (!ensureChildCapacity(parent)) {
            return nullptr;
        }

        struct TreeNode* node = &parent->children[parent->childrenCount];
        memset(node, 0, sizeof(TreeNode));

        node->name = new String(name);
        if (node->name == nullptr) {
            return nullptr;
        }

        node->isDirectory = isDirectory;

        parent->childrenCount++;
        return node;
    }


    /**
     * Adds the entries of node, a directory, with readdir, without reading the directories among them: each is read
     * when it's first expanded (click) or revealed (revealIn). Each entry's name is its full path, with a "/" after a
     * directory's.
     * @return the number of entries added, or -1 if the directory can't be read, which leaves it unread, so expanding
     * it again tries again
     */
    int loadDirectory(struct TreeNode* node){
        int fd = open(node->name->getData(), FS_FILE_FLAG_READ);
        if (fd <= -1) return -1;

        int added = 0;
        struct fs_dirent entry;

        while (readdir(fd, &entry) == 1) {
            int isDirectory = entry.attributes & FS_ATTR_DIRECTORY;

            String fullPath = node->name->getData();
            fullPath.concat(entry.name);
            if (isDirectory) {
                fullPath.concat("/");
            }

            if (insertNode(node, fullPath.getData(), isDirectory) != nullptr) {
                added++;
            }
        }

        fclose(fd);
        node->isLoaded = 1;
        return added;
    }


    void drawTreeRecursive(struct TreeNode* node, int depth, int* row, int parent_row, int max_rows, const char* highlight)
    {
        for (int i = 0; i < node->childrenCount; i++) {
            if (*row >= max_rows) {
                return;
            }

            struct TreeNode* child = &node->children[i];
            int current_row = *row;
            (*row)++;

            /* A row scrolled above the first shown isn't drawn, but its children still count */
            if (current_row >= 0 && !drawRow(child, depth, current_row, parent_row, highlight)) {
                return;
            }

            if (child->isExpanded && child->isDirectory) {
                drawTreeRecursive(child, depth + 1, row, current_row, max_rows, highlight);
            }
        }
    }

    /**
     * Draws child's row, current_row rows below the top, with the line from its parent's row, which is scrolled above
     * the top when parent_row is negative.
     * @return false when the row is past the box's bottom, so none after it fit either
     */
    bool drawRow(struct TreeNode* child, int depth, int current_row, int parent_row, const char* highlight)
    {
        int current_y = (current_row + 1) * 16;
        int current_x = depth * 16;
        int right = rowsRight();

        if (current_y + 15 >= height - 8) {
            return false;
        }

        /* The highlighted file's row, from just left of its icon to where rows end, under the line to it */
        if (!child->isDirectory && highlight != nullptr && pathMatches(highlight, child->name->getData(), false)) {
            int left = current_x - 2;
            if (right - 1 > left) {
                gfx_draw_rectangle(left, current_y, right - 1 - left, 16, TREE_VIEW_HIGHLIGHT);
            }
        }

        if (current_x + 16 <= right) {
            drawIcon(current_x, current_y, child->isDirectory ? &folderIcon : &fileIcon);
        }

        /**
         * Only the name after the last /, not counting a directory's own at the end. Searched in the full path:
         * moving name forward while counting j from its old start skipped slashes, so /lib/tcc/libtcc1.a showed
         * as tcc/libtcc1.a
         */
        const char* full = child->name->getData();
        const char* name = full;
        int full_len = strlen(full);
        for (int j = 0; j < full_len - 1; j++) {
            if (full[j] == '/') {
                name = full + j + 1;
            }
        }

        /* draw line from parent to current, from the box's top when the parent is scrolled above it */
        if (depth > 1) {
            int parent_y = parent_row >= 0 ? (parent_row + 1) * 16 + 8 : 13;
            int branch_x = (depth - 1) * 16 + 8;
            if (branch_x >= right) {
                branch_x = right - 1;
            }
            int line_to_x = current_x;
            if (line_to_x >= right) {
                line_to_x = right - 1;
            }

            /* Draw vertical line */
            gfx_draw_line(
                branch_x, // parent x + 8
                parent_y,
                branch_x,
                current_y + 8,
                COLOR_BLACK
            );

            /* Draw horizontal line */
            gfx_draw_line(
                branch_x,
                current_y + 8,
                line_to_x,
                current_y + 8,
                COLOR_BLACK
            );
        }

        drawClippedText(depth * 16 + 18, current_y + 5, name);
        return true;
    }
};


#endif /* !__ */
