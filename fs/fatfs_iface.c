/**
 * @file fatfs_iface.c
 * @brief The file system operations (struct filesystem_ops in fs.h) on FatFs (fs/fatfs), for FAT12, FAT16 and
 * FAT32 volumes, with long file names.
 *
 * fs.c normalizes paths before they get here: absolute, with no "." or "..", and no trailing slash.
 *
 * Two behaviors of the old FAT16 code that callers still depend on are kept, for now (#102):
 * - Each write replaces the file: it starts at the beginning, and the file ends where the write ends.
 * - Reading an open directory gives 32-byte FAT directory entries (struct fat16_directory_entry), with "." and ".."
 *   first in a subdirectory, because Finder and the Editor's tree view parse them. Their names are the 8.3 names.
 * A directory can't be opened for writing.
 */
#include <libc.h>
#include <fs/fs.h>
#include <fs/fat16.h>
#include <memory.h>
#include <terminal.h>
#include <timefmt.h>
#include <kutils.h>
#include <errors.h>
#include <serial.h>
#include <diskdev.h>

#include "fatfs/ff.h"

static FATFS fatfs_volume;

/* What an open file or directory keeps in struct file's data */
struct fatfs_open {
    int is_directory;
    FIL fil;
    /* A directory's entries, as the old FAT16 code returned them, read when it was opened */
    struct fat16_directory_entry* entries;
    int entry_count;
};

static int __fatfs_error(FRESULT res)
{
    switch(res){
    case FR_OK: return ERROR_OK;
    case FR_NO_FILE:
    case FR_NO_PATH: return -ERROR_FILE_NOT_FOUND;
    case FR_INVALID_NAME: return -ERROR_INVALID_ARGUMENTS;
    case FR_NOT_ENOUGH_CORE: return -ERROR_ALLOC;
    default: return -ERROR_UNKNOWN;
    }
}

static int __fatfs_is_root(const char* path)
{
    return path[0] == '/' && path[1] == 0;
}

/* Decodes a FAT date and time into the struct time the time formatters take */
static void __fatfs_time(WORD date, WORD time, struct time* out)
{
    int year = 1980 + (date >> 9);
    out->centuary = year / 100;
    out->year = year % 100;
    out->month = (date >> 5) & 0x0F;
    out->day = date & 0x1F;
    out->hour = time >> 11;
    out->minute = (time >> 5) & 0x3F;
    out->second = (time & 0x1F) * 2;
}

/* Spaces that pad text out to width characters, for lining up columns. The kernel's printf has no field widths. */
static const char* __padding(const char* text, int width)
{
    static const char spaces[] = "                ";
    int pad = width - strlen(text);

    if(pad < 0) pad = 0;
    if(pad > (int)sizeof(spaces) - 1) pad = sizeof(spaces) - 1;
    return spaces + (sizeof(spaces) - 1 - pad);
}

/* --- Directories, as the old FAT16 code read them --------------------------------------------------------------- */

static void __fatfs_dot_entry(struct fat16_directory_entry* entry, const char* name)
{
    memset(entry, 0, sizeof(*entry));
    memset(entry->full_name, ' ', 11);
    memcpy(entry->filename, name, strlen(name));
    entry->attributes = FAT16_FLAG_SUBDIRECTORY;
}

/**
 * An entry from FatFs's file information, with the 8.3 name as stored: "NAME.EXT", uppercase. That's altname, or
 * fname when altname is empty, which FatFs leaves it when the name has no long name and no lowercase flags.
 */
static void __fatfs_entry(struct fat16_directory_entry* entry, const FILINFO* info)
{
    memset(entry, 0, sizeof(*entry));
    memset(entry->full_name, ' ', 11);

    const char* name = info->altname[0] != 0 ? info->altname : info->fname;
    int i = 0;
    for(int j = 0; name[i] && name[i] != '.' && j < 8; i++, j++) entry->filename[j] = name[i];
    if(name[i] == '.') i++;
    for(int j = 0; name[i] && j < 3; i++, j++) entry->extension[j] = name[i];

    entry->attributes = info->fattrib;
    entry->file_size = info->fsize;
    entry->write_date = info->fdate;
    entry->write_time = info->ftime;
}

/**
 * Reads a directory's entries into open->entries: "." and ".." first in a subdirectory, then what FatFs lists.
 * The directory is read twice, once to count. If it gains entries in between, the extra ones are left out.
 */
static int __fatfs_read_directory(const char* path, struct fatfs_open* open)
{
    DIR dir;
    FILINFO info;
    int is_root = __fatfs_is_root(path);
    int count = is_root ? 0 : 2;

    if(f_opendir(&dir, path) != FR_OK) return -ERROR_FILE_NOT_FOUND;
    while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0) count++;
    f_closedir(&dir);

    open->is_directory = 1;
    open->entry_count = 0;
    if(count == 0) return ERROR_OK;

    open->entries = kalloc_try(count * sizeof(struct fat16_directory_entry));
    if(open->entries == NULL) return -ERROR_ALLOC;

    int i = 0;
    if(!is_root){
        __fatfs_dot_entry(&open->entries[i++], ".");
        __fatfs_dot_entry(&open->entries[i++], "..");
    }

    if(f_opendir(&dir, path) == FR_OK){
        while(i < count && f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0){
            __fatfs_entry(&open->entries[i++], &info);
        }
        f_closedir(&dir);
    }

    open->entry_count = i;
    return ERROR_OK;
}

/* --- Operations ----------------------------------------------------------------------------------------------- */

static struct file* fatfs_open(struct filesystem* fs, const char* path, int flags)
{
    if(fs == NULL || path == NULL) return NULL;

    FILINFO info;
    int is_root = __fatfs_is_root(path);
    int exists = is_root || f_stat(path, &info) == FR_OK;
    int is_directory = is_root || (exists && (info.fattrib & AM_DIR));

    /* A directory's entries aren't file data: writing them as a file would destroy the directory (#86) */
    if(is_directory && (flags & (FS_FILE_FLAG_WRITE | FS_FILE_FLAG_CREATE))) return NULL;
    if(!exists && !(flags & FS_FILE_FLAG_CREATE)) return NULL;

    struct fatfs_open* open = kalloc_try(sizeof(struct fatfs_open));
    if(open == NULL) return NULL;
    memset(open, 0, sizeof(struct fatfs_open));

    if(is_directory){
        if(__fatfs_read_directory(path, open) < 0){
            kfree(open);
            return NULL;
        }
    } else {
        BYTE mode = 0;
        if(flags & FS_FILE_FLAG_READ) mode |= FA_READ;
        if(flags & (FS_FILE_FLAG_WRITE | FS_FILE_FLAG_CREATE)) mode |= FA_WRITE;
        mode |= (flags & FS_FILE_FLAG_CREATE) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING;

        FRESULT res = f_open(&open->fil, path, mode);
        if(res != FR_OK){
            dbgprintf("[FATFS] Can't open %s: %d\n", path, res);
            kfree(open);
            return NULL;
        }
    }

    struct file* file = fs_alloc_file();
    if(file == NULL){
        if(is_directory){
            if(open->entries != NULL) kfree(open->entries);
        } else {
            f_close(&open->fil);
        }
        kfree(open);
        return NULL;
    }

    file->flags = flags;
    file->offset = 0;
    file->nlinks = 1;
    file->data = open;
    file->size = is_directory ? open->entry_count * (int)sizeof(struct fat16_directory_entry) : (int)f_size(&open->fil);

    return file;
}

static int fatfs_read(struct filesystem* fs, struct file* file, void* buf, int size)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    if(file->nlinks == 0 || file->data == NULL) return -1;
    if(!(file->flags & FS_FILE_FLAG_READ)) return -2;

    struct fatfs_open* open = file->data;
    if(open->is_directory){
        int total = open->entry_count * (int)sizeof(struct fat16_directory_entry);
        if(file->offset >= total) return 0;
        int n = size < total - file->offset ? size : total - file->offset;
        memcpy(buf, (char*)open->entries + file->offset, n);
        return n;
    }

    if(f_lseek(&open->fil, file->offset) != FR_OK) return -3;

    UINT read;
    if(f_read(&open->fil, buf, size, &read) != FR_OK) return -4;
    return read;
}

static int fatfs_write(struct filesystem* fs, struct file* file, const void* buf, int size)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    if(file->nlinks == 0 || file->data == NULL) return -1;
    if(!(file->flags & FS_FILE_FLAG_WRITE)) return -2;

    struct fatfs_open* open = file->data;
    if(open->is_directory) return -2;

    /**
     * Each write replaces the file, as the old FAT16 code did, until writes go to the file's position (#96).
     * f_sync puts the data and the new size on the disk now, since not every caller closes its files (#61).
     */
    UINT written;
    if(f_lseek(&open->fil, 0) != FR_OK) return -3;
    if(f_write(&open->fil, buf, size, &written) != FR_OK) return -4;
    if(f_truncate(&open->fil) != FR_OK) return -4;
    if(f_sync(&open->fil) != FR_OK) return -4;

    file->size = f_size(&open->fil);

    /* Fewer bytes than asked for means the disk is full */
    return written;
}

static int fatfs_close(struct filesystem* fs, struct file* file)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    if(file->nlinks == 0) return -1;

    struct fatfs_open* open = file->data;
    if(open != NULL){
        if(open->is_directory){
            if(open->entries != NULL) kfree(open->entries);
        } else {
            f_close(&open->fil);
        }
        kfree(open);
    }

    file->data = NULL;
    file->nlinks = 0;
    return 0;
}

static int fatfs_remove(struct filesystem* fs, const char* path)
{
    FS_VALIDATE(fs);

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);
    if(info.fattrib & AM_DIR) return -ERROR_INVALID_ARGUMENTS;

    return __fatfs_error(f_unlink(path));
}

static int fatfs_mkdir(struct filesystem* fs, const char* path)
{
    FS_VALIDATE(fs);
    return __fatfs_error(f_mkdir(path));
}

/* Removes an empty directory. FatFs refuses one that isn't. */
static int fatfs_rmdir(struct filesystem* fs, const char* path)
{
    FS_VALIDATE(fs);

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);
    if(!(info.fattrib & AM_DIR)) return -ERROR_INVALID_ARGUMENTS;

    return __fatfs_error(f_unlink(path));
}

/* Renames or moves a file or directory. The new path must not exist. */
static int fatfs_rename(struct filesystem* fs, const char* path, const char* new_path)
{
    FS_VALIDATE(fs);
    return __fatfs_error(f_rename(path, new_path));
}

static int fatfs_stat(struct filesystem* fs, const char* path, struct file* file)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);

    if(__fatfs_is_root(path)){
        file->size = 0;
        return ERROR_OK;
    }

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);

    file->size = info.fsize;
    return ERROR_OK;
}

/* The shell's list command: one line per entry, with its size, date, time and name */
static int fatfs_list(struct filesystem* fs, const char* path, char* buf, int size)
{
    FS_VALIDATE(fs);

    DIR dir;
    FILINFO info;
    int entries = 0;

    if(f_opendir(&dir, path) != FR_OK) return -ERROR_FILE_NOT_FOUND;

    /* The time is right-aligned, so 12-hour times line up */
    twritef("Size    Date%s %sTime  Name\n", __padding("Date", TIME_FORMAT_NUMERIC_DATE_MAX), __padding("Time", TIME_FORMAT_TIME_MAX));
    while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0){
        char date[TIME_FORMAT_DATE_MAX + 1] = "";
        char time[TIME_FORMAT_TIME_MAX + 1] = "";
        if(info.fdate != 0){
            struct time written;
            __fatfs_time(info.fdate, info.ftime, &written);
            time_format_date(date, sizeof(date), &written, TIME_DATE_NUMERIC);
            time_format_time(time, sizeof(time), &written, false);
        }

        struct unit size_unit = calculate_size_unit(info.fsize);

        twritef("%p%s %s%s %s%s  %s%s\n",
            size_unit.size,
            size_unit.unit,
            date, __padding(date, TIME_FORMAT_NUMERIC_DATE_MAX),
            __padding(time, TIME_FORMAT_TIME_MAX), time,
            info.fname,
            info.fattrib & AM_DIR ? "/" : ""
        );
        entries++;
    }
    f_closedir(&dir);

    twritef("%d directory entries.\n", entries);
    return 0;
}

static int __fatfs_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* Whether needle appears in name, ignoring case, as FAT does */
static int __fatfs_contains(const char* name, const char* needle)
{
    for(int i = 0; name[i] != 0; i++){
        int j = 0;
        while(needle[j] != 0 && __fatfs_lower(name[i + j]) == __fatfs_lower(needle[j])) j++;
        if(needle[j] == 0) return 1;
    }
    return needle[0] == 0;
}

#define FATFS_FIND_DEPTH 16

/* One level of find. Its state is on the heap: a kernel stack doesn't have room for 16 of them. */
struct fatfs_find_level {
    DIR dir;
    FILINFO info;
    char child[FS_PATH_MAX];
};

static void __fatfs_find(const char* path, const char* needle, int depth)
{
    struct fatfs_find_level* level = kalloc_try(sizeof(struct fatfs_find_level));
    if(level == NULL) return;

    if(f_opendir(&level->dir, path) == FR_OK){
        int root = __fatfs_is_root(path);
        while(f_readdir(&level->dir, &level->info) == FR_OK && level->info.fname[0] != 0){
            int is_directory = level->info.fattrib & AM_DIR;
            int path_len = strlen(path);
            int name_len = strlen(level->info.fname);
            if(path_len + 1 + name_len + 1 > FS_PATH_MAX) continue;

            memcpy(level->child, path, path_len);
            int len = path_len;
            if(!root) level->child[len++] = '/';
            memcpy(level->child + len, level->info.fname, name_len + 1);

            if(__fatfs_contains(level->info.fname, needle)){
                twritef("%s%s\n", level->child, is_directory ? "/" : "");
            }
            if(is_directory && depth < FATFS_FIND_DEPTH){
                __fatfs_find(level->child, needle, depth + 1);
            }
        }
        f_closedir(&level->dir);
    }

    kfree(level);
}

/* The shell's find command: every path under path whose name contains needle, ignoring case */
static int fatfs_find(struct filesystem* fs, char* path, const char* needle)
{
    FS_VALIDATE(fs);
    if(needle == NULL) return -ERROR_INVALID_ARGUMENTS;

    __fatfs_find(path, needle, 0);
    return 0;
}

static struct filesystem_ops fatfs_ops = {
    .write = fatfs_write,
    .read = fatfs_read,
    .open = fatfs_open,
    .close = fatfs_close,
    .remove = fatfs_remove,
    .mkdir = fatfs_mkdir,
    .rmdir = fatfs_rmdir,
    .rename = fatfs_rename,
    .stat = fatfs_stat,
    .list = fatfs_list,
    .find = fatfs_find
};

/* --- Mounting and formatting ------------------------------------------------------------------------------------ */

/* Mounts the disk's FAT volume, whether it starts at the first sector or in the first partition */
int fs_backend_mount(void)
{
    static struct filesystem* registered = NULL;

    FRESULT res = f_mount(&fatfs_volume, "", 1);
    if(res != FR_OK){
        dbgprintf("[FATFS] No FAT volume to mount: %d\n", res);
        return -1;
    }

    if(registered == NULL){
        registered = create(struct filesystem);
        ERR_ON_NULL(registered);
        registered->ops = &fatfs_ops;
        registered->flags = FS_FLAG_INITILIZED;
        registered->version = FS_VERSION;
        registered->type = fatfs_volume.fs_type;
        memcpy(registered->name, "fatfs", 6);
        fs_register(registered);
    }

    dbgprintf("[FATFS] Mounted a FAT%d volume\n", fatfs_volume.fs_type == FS_FAT12 ? 12 : fatfs_volume.fs_type == FS_FAT16 ? 16 : 32);
    return 0;
}

/* Where the boot sector keeps its copy of the volume label, and the field that's 0 only on FAT32 */
#define BOOT_LABEL_FAT16 43
#define BOOT_LABEL_FAT32 71
#define BOOT_FAT_SIZE16 22

/**
 * Formats the whole disk, with no partition table, as FAT12, FAT16 or FAT32: f_mkfs picks by the disk's size.
 * Two FATs, as other systems make them. The volume is left mounted, but not registered: fs_mount does that.
 *
 * A volume's label is kept twice, in the boot sector and in the root directory, and f_setlabel only writes the
 * second. So the boot sector's copy is written here first, while FatFs doesn't have the volume mounted.
 */
int fs_backend_format(const char* label, int reserved)
{
    (void)reserved;

    BYTE* work = kalloc_try(FF_MAX_SS);
    if(work == NULL) return -ERROR_ALLOC;

    MKFS_PARM options = { FM_ANY | FM_SFD, 2, 0, 0, 0 };
    FRESULT res = f_mkfs("", &options, work, FF_MAX_SS);
    if(res != FR_OK){
        kfree(work);
        dbgprintf("[FATFS] Format failed: %d\n", res);
        return __fatfs_error(res);
    }

    /* The label as FAT stores it: uppercase, padded with spaces to 11 characters */
    char padded[12];
    memset(padded, ' ', 11);
    padded[11] = 0;
    int len = 0;
    for(; label != NULL && label[len] != 0 && len < 11; len++){
        char c = label[len];
        padded[len] = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
    }

    if(len > 0 && read_block(work, 0) >= 0){
        int offset = (work[BOOT_FAT_SIZE16] | work[BOOT_FAT_SIZE16 + 1]) == 0 ? BOOT_LABEL_FAT32 : BOOT_LABEL_FAT16;
        memcpy(work + offset, padded, 11);
        write_block(work, 0);
    }
    kfree(work);

    res = f_mount(&fatfs_volume, "", 1);
    if(res == FR_OK && len > 0){
        padded[len] = 0;
        res = f_setlabel(padded);
    }
    return __fatfs_error(res);
}
