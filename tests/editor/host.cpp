/**
 * What the stub headers declare, for the Editor on the host: libgraphic's drawing, recorded in g_calls; events played
 * from g_events; the file system calls over files kept in memory, g_files; and the clipboard, held to CLIPBOARD_MAX as
 * the kernel's is (kernel/clipboard.c).
 */
#include "harness.hpp"

#include <errors.h>

std::vector<DrawCall> g_calls;
std::deque<struct gfx_event> g_events;
void (*g_beforeEventRead)() = nullptr;
std::map<std::string, std::string> g_files;
std::set<std::string> g_dirs;
std::set<std::string> g_unreadable;

int gfx_draw_text(int x, int y, const char* text, unsigned char color)
{
    g_calls.push_back({'t', x, y, 0, 0, color, text ? text : ""});
    return 0;
}

int gfx_draw_format_text(int x, int y, unsigned char color, const char* fmt, ...)
{
    char text[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    g_calls.push_back({'f', x, y, 0, 0, color, text});
    return 0;
}

int gfx_draw_char(int x, int y, char data, unsigned char color)
{
    g_calls.push_back({'c', x, y, 0, 0, color, std::string(1, data)});
    return 0;
}

int gfx_draw_rectangle(int x, int y, int width, int height, unsigned char color)
{
    g_calls.push_back({'r', x, y, width, height, color, ""});
    return 0;
}

int gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color)
{
    g_calls.push_back({'l', x0, y0, x1, y1, color, ""});
    return 0;
}

int gfx_draw_image(int x, int y, const struct image* image)
{
    g_calls.push_back({'i', x, y, image->width, image->height, 0, ""});
    return 0;
}

void gfx_draw_bevel(int x, int y, int width, int height, int pressed)
{
    g_calls.push_back({'b', x, y, width, height, pressed, ""});
}

/* A frame's end, recorded where it comes among the draw calls */
int gfx_end_frame(void)
{
    g_calls.push_back({'e', 0, 0, 0, 0, 0, ""});
    return 0;
}

/* Every icon is the library's placeholder size, with no pixels: nothing reads them, since drawing one is recorded */
int gfx_load_icon(const char* name, struct image* image)
{
    g_calls.push_back({'I', 0, 0, 0, 0, 0, name ? name : ""});
    image->width = 16;
    image->height = 16;
    image->pixels = nullptr;
    return 0;
}

void gfx_free_icon(struct image* image)
{
    (void)image;
}

/**
 * The next event in g_events, or with none left, the window closing, so EditorLoop ends. EDITOR_TEST_NO_EVENT stands
 * for a moment with nothing to read: a nonblocking read returns -1 for it, so a drag held past the text scrolls a
 * row, and a blocking read skips it.
 */
int gfx_get_event(struct gfx_event* event, gfx_event_flag_t flags)
{
    if (g_beforeEventRead != nullptr) {
        g_beforeEventRead();
    }

    while (!g_events.empty() && g_events.front().event == EDITOR_TEST_NO_EVENT) {
        g_events.pop_front();
        if (flags & GFX_EVENT_NONBLOCKING) {
            return -1;
        }
    }

    if (g_events.empty()) {
        *event = testEvent(GFX_EVENT_EXIT);
        return 0;
    }

    *event = g_events.front();
    g_events.pop_front();
    return 0;
}

/**
 * Files: a table of open ones, each a path and a position in g_files' text. A folder opened gets the names in it, as
 * readdir gives them, in name order
 */
struct OpenFile {
    bool used;
    std::string path;
    size_t position;
    std::vector<std::pair<std::string, bool>> entries;
};
static OpenFile files[32];

/* The entries of the folder dir, "/" or "/name/", by name, each with whether it's a folder; false if it isn't one */
static bool listFolder(const std::string& dir, std::vector<std::pair<std::string, bool>>& entries)
{
    std::map<std::string, bool> names;
    bool exists = dir == "/" || g_dirs.count(dir) > 0;
    auto add = [&](const std::string& path, bool is_dir) {
        if (path.size() <= dir.size() || path.compare(0, dir.size(), dir) != 0) {
            return;
        }
        exists = true;
        std::string rest = path.substr(dir.size());
        size_t slash = rest.find('/');
        if (slash == std::string::npos) {
            names[rest] = names[rest] || is_dir;
        } else if (slash > 0) {
            names[rest.substr(0, slash)] = true;
        }
    };
    for (auto& f : g_files) {
        add(f.first, false);
    }
    for (auto& d : g_dirs) {
        add(d, true);
    }
    entries.assign(names.begin(), names.end());
    return exists;
}

int open(const char* name, int flags)
{
    std::string path = name ? name : "";
    if (g_unreadable.count(path)) {
        return -ERROR_FILE_NOT_FOUND;
    }

    std::vector<std::pair<std::string, bool>> entries;
    if (!path.empty() && path.back() == '/') {
        if (!listFolder(path, entries)) {
            return -ERROR_FILE_NOT_FOUND;
        }
        for (int fd = 3; fd < 32; fd++) {
            if (!files[fd].used) {
                files[fd] = {true, path, 0, entries};
                return fd;
            }
        }
        return -ERROR_INDEX;
    }

    auto found = g_files.find(path);
    if (found == g_files.end()) {
        if (!(flags & FS_FILE_FLAG_CREATE)) {
            return -ERROR_FILE_NOT_FOUND;
        }
        g_files[path] = "";
    } else if (flags & FS_FILE_FLAG_TRUNCATE) {
        found->second.clear();
    }

    for (int fd = 3; fd < 32; fd++) {
        if (!files[fd].used) {
            files[fd] = {true, path, (flags & FS_FILE_FLAG_APPEND) ? g_files[path].size() : 0, {}};
            return fd;
        }
    }
    return -ERROR_INDEX;
}

static OpenFile* fileOf(int fd)
{
    return fd >= 3 && fd < 32 && files[fd].used ? &files[fd] : nullptr;
}

int read(int fd, void* buffer, int size)
{
    OpenFile* file = fileOf(fd);
    if (file == nullptr || size < 0) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    const std::string& text = g_files[file->path];
    size_t n = file->position < text.size() ? std::min((size_t)size, text.size() - file->position) : 0;
    memcpy(buffer, text.data() + file->position, n);
    file->position += n;
    g_calls.push_back({'R', fd, 0, (int)n, 0, 0, ""});
    return (int)n;
}

int write(int fd, void* buffer, int size)
{
    OpenFile* file = fileOf(fd);
    if (file == nullptr || size < 0) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    std::string& text = g_files[file->path];
    if (text.size() < file->position + size) {
        text.resize(file->position + size);
    }
    memcpy(&text[file->position], buffer, size);
    file->position += size;
    return size;
}

int fclose(int fd)
{
    OpenFile* file = fileOf(fd);
    if (file == nullptr) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    file->used = false;
    std::vector<std::pair<std::string, bool>>().swap(file->entries);
    return 0;
}

/* The next entry of a folder opened. @return 1 for one, 0 at the end */
int readdir(int fd, struct fs_dirent* out)
{
    OpenFile* file = fileOf(fd);
    if (file == nullptr) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    if (file->position >= file->entries.size()) {
        return 0;
    }
    auto& entry = file->entries[file->position++];
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", entry.first.c_str());
    out->attributes = entry.second ? FS_ATTR_DIRECTORY : 0;
    return 1;
}

int stat(const char* path, struct fs_dirent* out)
{
    auto found = g_files.find(path ? path : "");
    if (found == g_files.end()) {
        return -ERROR_FILE_NOT_FOUND;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", found->first.c_str());
    out->size = (uint32_t)found->second.size();
    return 0;
}

int getcwd(char* buf, int size)
{
    if (size < 2) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    strcpy(buf, "/");
    return 0;
}

void sleep(int ms)
{
    (void)ms;
}

/* The Editor's one direct system call, waiting for its Help thread, which never starts here */
extern "C" int invoke_syscall(int i, int arg1, int arg2, int arg3)
{
    (void)i;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 0;
}

void exit()
{
    throw EditorExit();
}

/* The clipboard, as kernel/clipboard.c keeps it */
static std::string clipboard;

int clipboard_set(const char* data, int size)
{
    if (size < 0 || size > CLIPBOARD_MAX || (data == nullptr && size > 0)) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    clipboard.assign(data ? data : "", size);
    return size;
}

int clipboard_get(char* data, int size)
{
    if (size < 0 || (data == nullptr && size > 0)) {
        return -ERROR_INVALID_ARGUMENTS;
    }
    memcpy(data, clipboard.data(), std::min((size_t)size, clipboard.size()));
    return (int)clipboard.size();
}

void clipboardReset()
{
    clipboard.clear();
}
