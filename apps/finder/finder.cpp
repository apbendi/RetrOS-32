/**
 * @file finder.cpp
 * @author Joe Bayer (joexbayer)
 * @brief A file manager app
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <libc.h>
#include <utils/Graphics.hpp>
#include <gfx/events.h>
#include <colors.h>
#include <utils/StringHelper.hpp>
#include <utils/StdLib.hpp>
#include <utils/Thread.hpp>
#include <fs/fs.h>
#include <utils/TreeView.hpp>
#include <lib/syscall.h>

#include <lib/printf.h>

#define WIDTH 350
#define HEIGHT 225
#define ICON_SIZE 32

#define TREE_VIEW_WIDTH 110  /* Width for the tree view panel */
#define FILE_CACHE_MAX 100   /* The most entries of a directory Finder keeps */
#define HEADER_HEIGHT 10     /* Height for the header */

class File {
public:
    File(const char* name, int flags, int x, int y, int w, int h) {
        this->name = new String(name);
        this->flags = flags;
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
    }

    ~File() {
        name->~String();
        delete name;
    }

    const char* getName() const {
        return name->getData();
    }


    int flags;
    String* name;
private:
    int x,y,w,h;
};

class FileCache {
public:
    FileCache() {
        m_size = 0;
    }

    ~FileCache() {
        clear();
    }

    int getSize() const {
        return m_size;
    }

    File* getByIndex(int index) const {
        return m_files[index];
    }

    File* getFile(const char* path) {
        for(int i = 0; i < m_size; i++){
            if(String::strcmp(m_files[i]->getName(), path) == 0){
                return m_files[i];
            }
        }
        return 0;
    }

    /* Takes the file, or deletes it if the cache is full. @return whether it was added */
    bool addFile(File* file) {
        if(m_size == FILE_CACHE_MAX){
            file->~File();
            delete file;
            return false;
        }
        m_files[m_size++] = file;
        return true;
    }

    void clear(){
        for(int i = 0; i < m_size; i++){
            File* file = (File*) m_files[i];
            file->~File();
            delete file;
        }
        m_size = 0;
    }

private:
    File* m_files[FILE_CACHE_MAX];
    int m_size = 0;
};


/* todo add to file */
class Finder : public Window
{
public:
    Finder() : Window(WIDTH, HEIGHT, "Finder", 0){
        drawRect(0, 0, WIDTH, HEIGHT, 30);

        treeView = new TreeView(0, 0, TREE_VIEW_WIDTH, HEIGHT);

        /* From /sysutil/icons, shared with the kernel. A file that can't be read gives a placeholder (#169) */
        gfx_load_icon("folder_32", &icon[0]);
        gfx_load_icon("text_32", &icon[1]);
        gfx_load_icon("cfile_32", &icon[2]);
        gfx_load_icon("bin_32", &icon[3]);

        path = new String("/");
        m_cache = new FileCache();

        setHeader(path->getData());
    }

    ~Finder(){
        for(int i = 0; i < 4; i++){
            gfx_free_icon(&icon[i]);
        }
        delete path;
        delete m_cache;
    }

    void drawIcon(int x, int y, const struct image* icon){
        drawImage(x, y, icon);
    }

    /* Whether name ends with suffix, ignoring case */
    static bool endsWith(const char* name, const char* suffix){
        int len = strlen(name);
        int suffix_len = strlen(suffix);
        if(suffix_len > len) return false;
        for(int i = 0; i < suffix_len; i++){
            char a = name[len - suffix_len + i];
            char b = suffix[i];
            if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
            if(a != b) return false;
        }
        return true;
    }

    /* Goes into a directory: ".." for the parent, or a name in the current one */
    void changeDirectory(const char* name){
        const char* current = path->getData();
        int len = path->getLength();

        /* Compared byte by byte: libc's strcmp only compares the first string's length */
        if(name[0] == '.' && name[1] == '.' && name[2] == 0){
            /* The path is absolute and has no trailing slash, so the parent is everything before the last "/" */
            int index = len - 1;
            while(index > 0 && current[index] != '/') index--;

            char* parent = new char[index + 2];
            memcpy(parent, current, index);
            parent[index] = 0;
            if(index == 0){
                parent[0] = '/';
                parent[1] = 0;
            }
            path->setString(parent);
            delete[] parent;
        } else {
            if(len > 1) path->concat("/");
            path->concat(name);
        }

        setHeader(path->getData());
        loadFiles();
        showFiles(0, 0);
    }

    /* Reads the current directory's entries, with ".." first below the root */
    int loadFiles(){
        m_cache->clear();
        m_entries = 0;

        if(path->getLength() > 1){
            m_cache->addFile(new File("..", FS_ATTR_DIRECTORY, 0, 0, 0, 0));
        }

        int fd = open(path->getData(), FS_FILE_FLAG_READ);
        if(fd < 0) return -1;

        struct fs_dirent entry;
        while (readdir(fd, &entry) == 1) {
            m_entries++;
            m_cache->addFile(new File(entry.name, entry.attributes, 0, 0, 0, 0));
        }

        fclose(fd);
        return 0;
    }

    int showFiles(int x, int y){

        if (x == 0 && y == 0) {
            /* Draw the main background */
            drawRect(TREE_VIEW_WIDTH, HEADER_HEIGHT, WIDTH - TREE_VIEW_WIDTH, HEIGHT - HEADER_HEIGHT, COLOR_WHITE);
            
            /* Draw the header */
            drawContouredRect(TREE_VIEW_WIDTH, 0, WIDTH - TREE_VIEW_WIDTH, HEADER_HEIGHT);

            /* Header text and other UI elements, adjusted for the new layout */
            gfx_draw_format_text(TREE_VIEW_WIDTH + 2, 2, COLOR_BLACK, "< >");
            /* The path, centered between "< >" and the count. A long one keeps its end, after "...". */
            int left = TREE_VIEW_WIDTH + 4*8;
            int right = WIDTH - strlen("XXX items")*8 - 8;
            int max_chars = (right - left) / 8;
            const char* shown = path->getData();
            int shown_len = path->getLength();
            const char* dots = "";
            if(shown_len > max_chars){
                shown += shown_len - (max_chars - 3);
                shown_len = max_chars;
                dots = "...";
            }
            gfx_draw_format_text(left + ((right - left) - shown_len*8)/2, 2, COLOR_BLACK, "%s%s", dots, shown);
            gfx_draw_format_text(WIDTH - strlen("XXX items")*8, 2, COLOR_BLACK, "%d items", m_entries);
        }
    
        File* file;
        int iter = 0;
        int j = 0, i = 0;
        int size = m_cache->getSize();
        while (iter < size){
            file = m_cache->getByIndex(iter);
            if(file == 0) continue;

            /* draw icon based on type */
            /* Adjust positioning for icons, considering the TREE_VIEW_WIDTH and HEADER_HEIGHT */
            int xOffset = TREE_VIEW_WIDTH + 4 + j * (WIDTH - TREE_VIEW_WIDTH) / 2;
            int yOffset = (HEADER_HEIGHT + 4 + i * ICON_SIZE);

            if (file->flags & FS_ATTR_DIRECTORY) {
                drawIcon(xOffset, yOffset, &icon[0]);
            } else if (endsWith(file->getName(), ".txt")) {
                drawIcon(xOffset, yOffset, &icon[1]);
            } else if (endsWith(file->getName(), ".c") || endsWith(file->getName(), ".h")) {
                drawIcon(xOffset, yOffset, &icon[2]);
            } else {
                drawIcon(xOffset, yOffset, &icon[3]);
            }

            /* Position text next to the icon, shortened with "..." to fit its column */
            int textXOffset = xOffset+ICON_SIZE+4;
            int textYOffset = yOffset + ICON_SIZE/2;
            drawName(textXOffset, textYOffset, file->getName(), ((WIDTH - TREE_VIEW_WIDTH)/2 - ICON_SIZE - 8) / 8);

            /* Check if x,y is inside box */
            if (x > xOffset && x < xOffset + ICON_SIZE && y > yOffset && y < yOffset + ICON_SIZE) {
                if (file->flags & FS_ATTR_DIRECTORY) {
                    changeDirectory(file->getName());
                    return 0;
                } else {
                    /* open file */
                }
            }

            if (i * ICON_SIZE > HEIGHT - ICON_SIZE-100) {
                i = 0;
                ++j;
            } else {
                ++i;
            }
            iter++;
        }

        return 0;
    }

    void drawName(int x, int y, const char* name, int max_chars){
        int len = strlen(name);
        if(len <= max_chars){
            drawText(x, y, name, COLOR_BLACK);
            return;
        }

        char shortened[64];
        int keep = max_chars - 3;
        if(keep > 60) keep = 60;
        if(keep < 0) keep = 0;
        memcpy(shortened, name, keep);
        memcpy(shortened + keep, "...", 4);
        drawText(x, y, shortened, COLOR_BLACK);
    }

    void Run(){
        /* event loop */
        loadFiles();

        drawRect(0, 0, WIDTH, HEIGHT, COLOR_WHITE);

        showFiles(0, 0);

        treeView->drawTree(this);

        struct gfx_event event;
        while (1){
            int ret = gfx_get_event(&event, GFX_EVENT_BLOCKING);
            if(ret == -1) continue;

            switch (event.event){
            case GFX_EVENT_MOUSE:{
                    /* check if a mouse click is inside of a file */
                    showFiles(event.data, event.data2);
                }
                break;
            case GFX_EVENT_EXIT:
                exit();
                break;
            default:
                break;
            }
        }
    }

private:
    /* The directory's entries, not counting ".." */
    int m_entries = 0;
    /* Folder, text, C and other files' icons */
    struct image icon[4];
    FileCache* m_cache;
    String* path;
    TreeView* treeView;
};

int main(void)
{
    Finder finder;
    finder.Run();

    return 0;
}
