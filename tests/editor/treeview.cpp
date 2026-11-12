/**
 * Host tests of the real apps/utils/TreeView.hpp, which the Editor's tests stub (#214 step 3): a folder is read again
 * each time it's opened, and refresh() reads the tree again keeping open the folders that were. The folders are the
 * harness's in-memory file system. Built by run.sh, with TREEVIEW_HPP its path.
 */
#include <cstdint>
#include <cstdlib>
#include <new>

/**
 * Live allocations made while g_tracking is set, to show none are left once the tree is gone. They're kept in a table
 * of pointers, so a free takes one off only if it was counted, whoever allocated it: the C++ library allocates some of
 * its own. The tests change the files with g_tracking off, since the in-memory file system allocates too
 */
static bool g_tracking = true;
static long g_live = 0;
static const size_t TRACK_SLOTS = 1 << 18;
static void* g_tracked[TRACK_SLOTS];
static void* const TRACK_GONE = (void*)1;

static size_t trackSlot(void* p)
{
    return (((uintptr_t)p >> 4) * 2654435761u) & (TRACK_SLOTS - 1);
}

static void track(void* p)
{
    for (size_t i = trackSlot(p);; i = (i + 1) & (TRACK_SLOTS - 1)) {
        if (g_tracked[i] == nullptr || g_tracked[i] == TRACK_GONE) {
            g_tracked[i] = p;
            return;
        }
    }
}

static bool untrack(void* p)
{
    for (size_t i = trackSlot(p);; i = (i + 1) & (TRACK_SLOTS - 1)) {
        if (g_tracked[i] == nullptr) {
            return false;
        }
        if (g_tracked[i] == p) {
            g_tracked[i] = TRACK_GONE;
            return true;
        }
    }
}

void* operator new(size_t size)
{
    void* p = malloc(size ? size : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    if (g_tracking) {
        track(p);
        g_live++;
    }
    return p;
}
void* operator new[](size_t size)
{
    return operator new(size);
}
void operator delete(void* p) noexcept
{
    if (p != nullptr) {
        if (untrack(p)) {
            g_live--;
        }
        free(p);
    }
}
void operator delete[](void* p) noexcept
{
    operator delete(p);
}
void operator delete(void* p, size_t) noexcept
{
    operator delete(p);
}
void operator delete[](void* p, size_t) noexcept
{
    operator delete(p);
}

#define private public
#include TREEVIEW_HPP
#undef private

#include "harness.hpp"

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        g_checks++;                                                         \
        if (!(cond)) {                                                      \
            g_failures++;                                                   \
            printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond);          \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

/* The rows the tree shows, top to bottom, as "  name" indented two spaces a level, open folders marked "+" */
static std::string rows(TreeView* tree)
{
    std::string out;
    int total = tree->countRows(&tree->root);
    for (int i = 0; i < total; i++) {
        int row = 0;
        TreeView::TreeNode* node = tree->findNodeByRow(&tree->root, &row, i);
        if (node == nullptr) {
            out += "?;";
            continue;
        }
        const char* full = node->name->getData();
        int depth = 0;
        int len = (int)strlen(full);
        for (int j = 1; j < len - 1; j++) {
            depth += full[j] == '/';
        }
        out += std::string(depth * 2, ' ') + (node->isExpanded ? "+" : "") + full + ";";
    }
    return out;
}

/* Clicks the row whose full name is name, as the user would, with every row in view */
static void clickRow(TreeView* tree, const char* name)
{
    int total = tree->countRows(&tree->root);
    for (int i = 0; i < total; i++) {
        int row = 0;
        TreeView::TreeNode* node = tree->findNodeByRow(&tree->root, &row, i);
        if (node != nullptr && strcmp(node->name->getData(), name) == 0) {
            tree->click(20, tree->rowY(i - tree->firstRow()) + 2);
            return;
        }
    }
    CHECK(false, "no row %s to click in %s", name, rows(tree).c_str());
}

/* Changes the files, uncounted */
#define FILES(change) do { g_tracking = false; change; g_tracking = true; } while (0)

static void files(std::initializer_list<const char*> paths)
{
    g_tracking = false;
    g_files.clear();
    g_dirs.clear();
    g_unreadable.clear();
    for (const char* p : paths) {
        g_files[p] = "x";
    }
    g_tracking = true;
}

static void testReading()
{
    files({"/a.c", "/lib/x.c", "/lib/sub/y.c", "/usr/z.h"});
    FILES(g_dirs.insert("/empty/"));
    long before = g_live;
    TreeView* tree = new TreeView(0, 0, 130, 2000);

    CHECK(rows(tree) == "/a.c;/empty/;/lib/;/usr/;", "made: %s", rows(tree).c_str());
    CHECK(!tree->root.children[2].isLoaded, "a folder read before it's opened");

    /* Opening a folder reads it */
    clickRow(tree, "/lib/");
    CHECK(rows(tree) == "/a.c;/empty/;+/lib/;  /lib/sub/;  /lib/x.c;/usr/;", "lib opened: %s", rows(tree).c_str());

    /* A file added, then deleted, while it's closed: opening it again shows what's there now */
    clickRow(tree, "/lib/");
    FILES(g_files["/lib/new.c"] = "x");
    clickRow(tree, "/lib/");
    CHECK(rows(tree) == "/a.c;/empty/;+/lib/;  /lib/new.c;  /lib/sub/;  /lib/x.c;/usr/;", "lib opened again: %s", rows(tree).c_str());
    clickRow(tree, "/lib/");
    FILES(g_files.erase("/lib/x.c"));
    clickRow(tree, "/lib/");
    CHECK(rows(tree) == "/a.c;/empty/;+/lib/;  /lib/new.c;  /lib/sub/;/usr/;", "x.c deleted: %s", rows(tree).c_str());

    /* Closing and opening a folder keeps the folders inside it open, read again too */
    clickRow(tree, "/lib/sub/");
    clickRow(tree, "/lib/");
    FILES(g_files["/lib/sub/k.c"] = "x");
    clickRow(tree, "/lib/");
    CHECK(rows(tree) == "/a.c;/empty/;+/lib/;  /lib/new.c;  +/lib/sub/;    /lib/sub/k.c;    /lib/sub/y.c;/usr/;",
          "sub kept open: %s", rows(tree).c_str());

    /* A folder that can't be read when it's opened stays as it was */
    clickRow(tree, "/lib/");
    FILES(g_unreadable.insert("/lib/"));
    FILES(g_files["/lib/later.c"] = "x");
    clickRow(tree, "/lib/");
    CHECK(rows(tree) == "/a.c;/empty/;+/lib/;  /lib/new.c;  +/lib/sub/;    /lib/sub/k.c;    /lib/sub/y.c;/usr/;",
          "lib unreadable: %s", rows(tree).c_str());
    FILES(g_unreadable.clear());

    delete tree;
    CHECK(g_live == before, "%ld allocations left", g_live - before);
}

static void testRefresh()
{
    files({"/a.c", "/lib/x.c", "/lib/sub/y.c", "/usr/z.h", "/usr/include/w.h"});
    long before = g_live;
    TreeView* tree = new TreeView(0, 0, 130, 2000);
    clickRow(tree, "/lib/");
    clickRow(tree, "/lib/sub/");

    /* Changes in open and closed folders, and at the top */
    FILES(g_files["/b.c"] = "x");
    FILES(g_files["/lib/sub/k.c"] = "x");
    FILES(g_files["/usr/q.h"] = "x");
    FILES(g_files.erase("/a.c"));
    CHECK(tree->refresh(), "refresh failed");
    CHECK(rows(tree) == "/b.c;+/lib/;  +/lib/sub/;    /lib/sub/k.c;    /lib/sub/y.c;  /lib/x.c;/usr/;", "refreshed: %s", rows(tree).c_str());
    clickRow(tree, "/usr/");
    CHECK(rows(tree) == "/b.c;+/lib/;  +/lib/sub/;    /lib/sub/k.c;    /lib/sub/y.c;  /lib/x.c;+/usr/;  /usr/include/;  /usr/q.h;  /usr/z.h;",
          "usr opened after: %s", rows(tree).c_str());

    /* A folder deleted while it's open goes, and the folder around it stays open */
    FILES(g_files.erase("/lib/sub/k.c"));
    FILES(g_files.erase("/lib/sub/y.c"));
    CHECK(tree->refresh(), "refresh failed");
    CHECK(rows(tree) == "/b.c;+/lib/;  /lib/x.c;+/usr/;  /usr/include/;  /usr/q.h;  /usr/z.h;", "sub deleted: %s", rows(tree).c_str());

    /* An open folder that can't be read keeps what it had; the root that can't be read leaves everything */
    FILES(g_unreadable.insert("/usr/"));
    FILES(g_files["/usr/r.h"] = "x");
    CHECK(tree->refresh(), "refresh failed");
    CHECK(rows(tree) == "/b.c;+/lib/;  /lib/x.c;+/usr/;  /usr/include/;  /usr/q.h;  /usr/z.h;", "usr unreadable: %s", rows(tree).c_str());
    FILES(g_unreadable.clear());
    FILES(g_unreadable.insert("/"));
    FILES(g_files["/c.c"] = "x");
    CHECK(!tree->refresh(), "a refresh of an unreadable root succeeded");
    CHECK(rows(tree) == "/b.c;+/lib/;  /lib/x.c;+/usr/;  /usr/include/;  /usr/q.h;  /usr/z.h;", "root unreadable: %s", rows(tree).c_str());
    FILES(g_unreadable.clear());

    delete tree;
    CHECK(g_live == before, "%ld allocations left", g_live - before);
}

/* A refresh keeps the scroll position, which the next draw keeps within the rows there are */
static void testScroll()
{
    files({});
    for (int i = 0; i < 40; i++) {
        char name[32];
        snprintf(name, sizeof(name), "/f%02d.c", i);
        g_files[name] = "x";
    }
    TreeView* tree = new TreeView(0, 0, 130, 200);
    tree->setScrollable(true);
    Window window(130, 200, "test", 0);
    tree->drawTree(&window);
    tree->scrollBar.set(tree->countRows(&tree->root), tree->visibleRows(), 20);
    CHECK(tree->firstRow() == 20, "scrolled to %d", tree->firstRow());

    FILES(g_files["/a0.c"] = "x");
    tree->refresh();
    tree->drawTree(&window);
    CHECK(tree->firstRow() == 20, "after a refresh, scrolled to %d", tree->firstRow());

    for (int i = 0; i < 35; i++) {
        char name[32];
        snprintf(name, sizeof(name), "/f%02d.c", i);
        FILES(g_files.erase(name));
    }
    tree->refresh();
    tree->drawTree(&window);
    int max = tree->countRows(&tree->root) - tree->visibleRows();
    CHECK(tree->firstRow() == (max > 0 ? max : 0), "after most rows went, scrolled to %d of at most %d", tree->firstRow(), max);
    delete tree;
}

/**
 * Each row's icon, by how the file's name ends, in any case: C and header files, text, web pages, settings and images
 * have their own, and everything else, .sh, .o and .out included, the plain file's. A folder is a folder, whatever its
 * name
 */
static void testIcons()
{
    static const struct {
        const char* path;
        const char* icon;
    } cases[] = {
        {"/a.c", "cfile_16"},      {"/B.C", "cfile_16"},     {"/c.h", "hfile_16"},     {"/D.H", "hfile_16"},
        {"/e.txt", "text_16"},     {"/F.Txt", "text_16"},    {"/g.htm", "html_16"},    {"/h.html", "html_16"},
        {"/I.HTML", "html_16"},    {"/j.cfg", "cfg_16"},     {"/k.bmp", "image_16"},   {"/L.BMP", "image_16"},
        {"/m.sh", "file_16"},      {"/n.o", "file_16"},      {"/o.out", "file_16"},    {"/p", "file_16"},
        {"/qc", "file_16"},        {"/r.cc", "file_16"},     {"/s.cfgx", "file_16"},   {"/t.c.txt", "text_16"},
        {"/u.html.c", "cfile_16"}, {"/v.htmlx", "file_16"},  {"/w.txt/", "folder_16"},
    };
    files({});
    for (const auto& c : cases) {
        int len = (int)strlen(c.path);
        if (c.path[len - 1] == '/') {
            FILES(g_dirs.insert(c.path));
        } else {
            FILES(g_files[c.path] = "x");
        }
    }
    TreeView* tree = new TreeView(0, 0, 130, 2000);
    Window window(130, 2000, "test", 0);
    g_calls.clear();
    tree->drawTree(&window);

    int total = tree->countRows(&tree->root);
    CHECK(total == (int)(sizeof(cases) / sizeof(cases[0])), "%d rows: %s", total, rows(tree).c_str());
    for (int i = 0; i < total; i++) {
        int row = 0;
        TreeView::TreeNode* node = tree->findNodeByRow(&tree->root, &row, i);
        const char* name = node != nullptr ? node->name->getData() : "?";
        std::string icon = "none";
        for (const DrawCall& call : g_calls) {
            if (call.kind == 'i' && call.y == tree->rowY(i - tree->firstRow())) {
                icon = call.text;
            }
        }
        const char* expected = "?";
        for (const auto& c : cases) {
            expected = strcmp(c.path, name) == 0 ? c.icon : expected;
        }
        CHECK(icon == expected, "%s drawn with %s, not %s", name, icon.c_str(), expected);
    }
    delete tree;
}

int main()
{
    /* So recording draw calls doesn't allocate while counting */
    g_tracking = false;
    g_calls.reserve(1 << 16);
    g_tracking = true;

    testReading();
    testRefresh();
    testScroll();
    testIcons();
    printf("TreeView: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures != 0;
}
