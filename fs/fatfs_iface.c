/**
 * @file fatfs_iface.c
 * @brief The file system operations (struct filesystem_ops in fs.h) on FatFs (fs/fatfs), for FAT12, FAT16 and
 * FAT32 volumes, with long file names.
 *
 * fs.c normalizes paths before they get here: absolute, with no "." or "..", and no trailing slash.
 *
 * Reads and writes go to the file's position (struct file's offset, which fs.c moves), or to its end with
 * FS_FILE_FLAG_APPEND. A write can't start past the end of the file. FS_FILE_FLAG_TRUNCATE empties a file as
 * it's opened.
 *
 * A directory is opened to list it with readdir, which gives long names and leaves out "." and "..". It can't be
 * opened for writing, and reading it like a file is refused.
 */
#include <libc.h>
#include <fs/fs.h>
#include <memory.h>
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
    /* A directory, for fs_readdir */
    DIR dir;
};

/**
 * RetrOS's error code for a FatFs result. FatFs says FR_DENIED both when a directory or the disk is full and when
 * a directory to remove isn't empty, so rmdir checks for the second itself.
 */
static int __fatfs_error(FRESULT res)
{
    switch(res){
    case FR_OK: return ERROR_OK;
    case FR_NO_FILE: return -ERROR_FILE_NOT_FOUND;
    case FR_NO_PATH: return -ERROR_PATH_NOT_FOUND;
    case FR_INVALID_NAME: return -ERROR_INVALID_NAME;
    case FR_EXIST: return -ERROR_FILE_EXISTS;
    case FR_DENIED: return -ERROR_NO_SPACE;
    case FR_NOT_ENOUGH_CORE: return -ERROR_ALLOC;
    case FR_WRITE_PROTECTED:
    case FR_LOCKED: return -ERROR_ACCESS_DENIED;
    case FR_DISK_ERR:
    case FR_INT_ERR:
    case FR_NOT_READY: return -ERROR_DISK;
    case FR_NO_FILESYSTEM:
    case FR_NOT_ENABLED:
    case FR_INVALID_DRIVE: return -ERROR_NO_FILESYSTEM;
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

/* --- Operations ----------------------------------------------------------------------------------------------- */

/* Sets *error, if there's somewhere to set it, and returns NULL */
static struct file* __fatfs_open_failed(int* error, int code)
{
    if(error != NULL) *error = code;
    return NULL;
}

static struct file* fatfs_open(struct filesystem* fs, const char* path, int flags, int* error)
{
    if(fs == NULL || path == NULL) return __fatfs_open_failed(error, -ERROR_NULL_POINTER);

    FILINFO info;
    int is_root = __fatfs_is_root(path);
    FRESULT found = is_root ? FR_OK : f_stat(path, &info);
    int exists = found == FR_OK;
    int is_directory = is_root || (exists && (info.fattrib & AM_DIR));

    /* A directory's entries aren't file data: writing them as a file would destroy the directory (#86) */
    int writes = flags & (FS_FILE_FLAG_WRITE | FS_FILE_FLAG_CREATE | FS_FILE_FLAG_TRUNCATE | FS_FILE_FLAG_APPEND);
    if(is_directory && writes) return __fatfs_open_failed(error, -ERROR_IS_DIRECTORY);
    if(!exists && !(flags & FS_FILE_FLAG_CREATE)) return __fatfs_open_failed(error, __fatfs_error(found));

    struct fatfs_open* open = kalloc_try(sizeof(struct fatfs_open));
    if(open == NULL) return __fatfs_open_failed(error, -ERROR_ALLOC);
    memset(open, 0, sizeof(struct fatfs_open));

    if(is_directory){
        FRESULT res = f_opendir(&open->dir, path);
        if(res != FR_OK){
            kfree(open);
            return __fatfs_open_failed(error, __fatfs_error(res));
        }
        open->is_directory = 1;
    } else {
        BYTE mode = 0;
        if(flags & FS_FILE_FLAG_READ) mode |= FA_READ;
        if(writes) mode |= FA_WRITE;
        mode |= (flags & FS_FILE_FLAG_CREATE) ? FA_OPEN_ALWAYS : FA_OPEN_EXISTING;

        FRESULT res = f_open(&open->fil, path, mode);
        if(res == FR_OK && (flags & FS_FILE_FLAG_TRUNCATE)){
            res = f_truncate(&open->fil);
            if(res == FR_OK) res = f_sync(&open->fil);
            if(res != FR_OK) f_close(&open->fil);
        }
        if(res != FR_OK){
            dbgprintf("[FATFS] Can't open %s: %d\n", path, res);
            kfree(open);
            return __fatfs_open_failed(error, __fatfs_error(res));
        }
    }

    struct file* file = fs_alloc_file();
    if(file == NULL){
        if(is_directory){
            f_closedir(&open->dir);
        } else {
            f_close(&open->fil);
        }
        kfree(open);
        return __fatfs_open_failed(error, -ERROR_INDEX);
    }

    file->flags = flags;
    file->offset = 0;
    file->nlinks = 1;
    file->data = open;
    file->size = is_directory ? 0 : (int)f_size(&open->fil);

    return file;
}

static int fatfs_read(struct filesystem* fs, struct file* file, void* buf, int size)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    if(file->nlinks == 0 || file->data == NULL) return -ERROR_INVALID_ARGUMENTS;
    if(!(file->flags & FS_FILE_FLAG_READ)) return -ERROR_ACCESS_DENIED;

    struct fatfs_open* open = file->data;
    if(open->is_directory) return -ERROR_IS_DIRECTORY;

    FRESULT res = f_lseek(&open->fil, file->offset);
    if(res != FR_OK) return __fatfs_error(res);

    UINT read;
    res = f_read(&open->fil, buf, size, &read);
    if(res != FR_OK) return __fatfs_error(res);
    return read;
}

static int fatfs_write(struct filesystem* fs, struct file* file, const void* buf, int size)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    if(file->nlinks == 0 || file->data == NULL) return -ERROR_INVALID_ARGUMENTS;
    if(!(file->flags & FS_FILE_FLAG_WRITE)) return -ERROR_ACCESS_DENIED;

    struct fatfs_open* open = file->data;
    if(open->is_directory) return -ERROR_IS_DIRECTORY;

    /**
     * Past the end of the file, FatFs would leave whatever the disk held before in the gap, so that's refused.
     * f_sync puts the data and the new size on the disk now, so nothing's lost if the machine stops before the
     * file is closed.
     */
    DWORD position = (file->flags & FS_FILE_FLAG_APPEND) ? f_size(&open->fil) : (DWORD)file->offset;
    if(position > f_size(&open->fil)) return -ERROR_INVALID_ARGUMENTS;

    UINT written;
    FRESULT res = f_lseek(&open->fil, position);
    if(res == FR_OK) res = f_write(&open->fil, buf, size, &written);
    if(res == FR_OK) res = f_sync(&open->fil);
    if(res != FR_OK) return __fatfs_error(res);

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
            f_closedir(&open->dir);
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

    if(__fatfs_is_root(path)) return -ERROR_IS_DIRECTORY;

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);
    if(info.fattrib & AM_DIR) return -ERROR_IS_DIRECTORY;

    return __fatfs_error(f_unlink(path));
}

static int fatfs_mkdir(struct filesystem* fs, const char* path)
{
    FS_VALIDATE(fs);
    return __fatfs_error(f_mkdir(path));
}

/* Removes an empty directory. FatFs refuses one that isn't, with FR_DENIED. */
static int fatfs_rmdir(struct filesystem* fs, const char* path)
{
    FS_VALIDATE(fs);
    if(__fatfs_is_root(path)) return -ERROR_ACCESS_DENIED;

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);
    if(!(info.fattrib & AM_DIR)) return -ERROR_NOT_DIRECTORY;

    res = f_unlink(path);
    return res == FR_DENIED ? -ERROR_DIRECTORY_NOT_EMPTY : __fatfs_error(res);
}

/**
 * Whether new_path is inside the directory at path, so that moving it there would cut it off from the root, which
 * FatFs doesn't check. The directories are compared by their first cluster, so a path that spells a name differently,
 * in another case or by its 8.3 name, is still caught.
 */
static int __fatfs_moves_into_itself(const char* path, const char* new_path)
{
    DIR dir;
    if(f_opendir(&dir, path) != FR_OK) return 0;
    DWORD cluster = dir.obj.sclust;
    f_closedir(&dir);

    /* Each directory new_path goes through, from the top: /a, then /a/b, but not new_path itself */
    char prefix[FS_PATH_MAX];
    for (int i = 1; new_path[i] != 0 && i < FS_PATH_MAX; i++){
        if(new_path[i] != '/') continue;

        memcpy(prefix, new_path, i);
        prefix[i] = 0;
        if(f_opendir(&dir, prefix) != FR_OK) return 0;
        DWORD inside = dir.obj.sclust;
        f_closedir(&dir);

        if(inside == cluster) return 1;
    }
    return 0;
}

/* Renames or moves a file or directory. The new path must not exist, or be inside the directory being moved. */
static int fatfs_rename(struct filesystem* fs, const char* path, const char* new_path)
{
    FS_VALIDATE(fs);
    if(__fatfs_moves_into_itself(path, new_path)) return -ERROR_MOVE_INTO_ITSELF;
    return __fatfs_error(f_rename(path, new_path));
}

/* Where a file or directory is on the disk, which no two share */
struct fatfs_place {
    int is_directory;
    /* A directory's first cluster, 0 for the root. A file's won't do: an empty file has none. */
    DWORD cluster;
    /* A file's directory entry: its sector, and its offset in that sector */
    LBA_t sector;
    UINT offset;
};

/* Finds where path's file or directory is. @return 0, or a negative error code */
static int __fatfs_locate(const char* path, struct fatfs_place* place)
{
    memset(place, 0, sizeof(*place));

    DIR dir;
    if(f_opendir(&dir, path) == FR_OK){
        place->is_directory = 1;
        place->cluster = dir.obj.sclust;
        f_closedir(&dir);
        return ERROR_OK;
    }

    FIL* file = kalloc_try(sizeof(FIL));
    if(file == NULL) return -ERROR_ALLOC;

    FRESULT res = f_open(file, path, FA_READ);
    if(res == FR_OK){
        place->sector = file->dir_sect;
        place->offset = file->dir_ptr - file->obj.fs->win;
        f_close(file);
    }
    kfree(file);
    return __fatfs_error(res);
}

/* Whether path and other name the same file or directory, compared by where it is, so any spelling of a name works */
static int fatfs_same(struct filesystem* fs, const char* path, const char* other)
{
    FS_VALIDATE(fs);

    struct fatfs_place a, b;
    int ret = __fatfs_locate(path, &a);
    if(ret < 0) return ret;
    ret = __fatfs_locate(other, &b);
    if(ret < 0) return ret;

    return a.is_directory == b.is_directory && a.cluster == b.cluster && a.sector == b.sector && a.offset == b.offset;
}

static void __fatfs_dirent(const FILINFO* info, struct fs_dirent* out)
{
    memset(out, 0, sizeof(*out));
    int len = strlen(info->fname);
    if(len > FS_NAME_MAX) len = FS_NAME_MAX;
    memcpy(out->name, info->fname, len);
    out->name[len] = 0;
    out->size = info->fsize;
    out->attributes = info->fattrib;
    if(info->fdate != 0) __fatfs_time(info->fdate, info->ftime, &out->written);
}

/**
 * f_stat reports a long name as the path spelled it, not as it's stored: it copies the name it searched for. The
 * 8.3 name it reports is the stored one, and unique in the directory, so this finds the entry in its directory by
 * that and takes the stored long name from there. A name with no long name and no case flags has no altname, and
 * its fname is already the stored one.
 */
static void __fatfs_stored_name(const char* path, FILINFO* info)
{
    if(info->altname[0] == 0) return;

    char parent[FS_PATH_MAX];
    int len = strlen(path);
    while(len > 0 && path[len - 1] != '/') len--;
    if(len <= 1){
        parent[0] = '/';
        parent[1] = 0;
    } else {
        memcpy(parent, path, len - 1);
        parent[len - 1] = 0;
    }

    DIR dir;
    FILINFO entry;
    if(f_opendir(&dir, parent) != FR_OK) return;
    while(f_readdir(&dir, &entry) == FR_OK && entry.fname[0] != 0){
        /* Not strcmp: the kernel's only compares as many characters as the first string has */
        int alt_len = strlen(entry.altname);
        if(alt_len == strlen(info->altname) && memcmp(entry.altname, info->altname, alt_len) == 0){
            memcpy(info->fname, entry.fname, sizeof(info->fname));
            break;
        }
    }
    f_closedir(&dir);
}

static int fatfs_stat(struct filesystem* fs, const char* path, struct fs_dirent* out)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(out);

    /* The root directory has no entry of its own */
    if(__fatfs_is_root(path)){
        memset(out, 0, sizeof(*out));
        out->name[0] = '/';
        out->attributes = FS_ATTR_DIRECTORY;
        return ERROR_OK;
    }

    FILINFO info;
    FRESULT res = f_stat(path, &info);
    if(res != FR_OK) return __fatfs_error(res);

    __fatfs_stored_name(path, &info);
    __fatfs_dirent(&info, out);
    return ERROR_OK;
}

static int fatfs_readdir(struct filesystem* fs, struct file* file, struct fs_dirent* out)
{
    FS_VALIDATE(fs);
    ERR_ON_NULL(file);
    ERR_ON_NULL(out);

    struct fatfs_open* open = file->data;
    if(file->nlinks == 0 || open == NULL) return -ERROR_INVALID_ARGUMENTS;
    if(!open->is_directory) return -ERROR_NOT_DIRECTORY;

    FILINFO info;
    FRESULT res = f_readdir(&open->dir, &info);
    if(res != FR_OK) return __fatfs_error(res);
    if(info.fname[0] == 0) return 0;

    __fatfs_dirent(&info, out);
    return 1;
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
    .same = fatfs_same,
    .stat = fatfs_stat,
    .readdir = fatfs_readdir
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

int fs_backend_usage(int* used_kb, int* total_kb)
{
    DWORD free_clusters;
    FATFS* volume;
    if(f_getfree("", &free_clusters, &volume) != FR_OK) return -1;

    DWORD clusters = volume->n_fatent - 2;
    *total_kb = (int)(clusters * volume->csize / 2);
    *used_kb = (int)((clusters - free_clusters) * volume->csize / 2);
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
