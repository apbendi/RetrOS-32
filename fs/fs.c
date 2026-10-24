/**
 * @file fs.c
 * @author Joe Bayer (joexbayer)
 * @brief In memory Filesystem abstraction implementation
 * @version 0.1
 * @date 2023-08-30
 * 
 * @copyright Copyright (c) 2023
 * 
 * The abstraction layer which is used by the kernel to access the filesystem.
 * Hides which filesystem is used, and provides a common interface for the kernel.
 * Controls the open files and their offsets, this also a "file" in the filesystem can have multiple
 * open in memory files. As a file only cares about the offset and the flags.
 * 
 */

#include <fs/fs.h>
#include <assert.h>
#include <serial.h>
#include <errors.h>
#include <pcb.h>
#include <sync.h>

/* This determines the maximum of simultaneously open files */
#define FS_MAX_FILES 256
#define FS_MAX_FILESYSTEMS 4 /* maximum number of partitons entries in MBR */

/* default filesystem struct */
static struct file fs_file_table[FS_MAX_FILES];
static struct filesystem* fs_table[FS_MAX_FILESYSTEMS] = {NULL};

/**
 * File descriptors are per process: fs_process_files[pid] maps a process's descriptors to open files in
 * fs_file_table, and a thread uses its process's. 0 to 2 are left for standard input and output, so no file is
 * ever fd 0, which programs use to mean "no file" (#81). Files opened before any process runs, at boot, go in
 * fs_boot_files. A process's files are closed when it exits (fs_close_files_of).
 */
#define FS_FIRST_FD 3
static struct file* fs_process_files[MAX_NUM_OF_PCBS][FS_MAX_PROCESS_FILES];
static struct file* fs_boot_files[FS_MAX_PROCESS_FILES];

/* Guards fs_file_table and every table of descriptors. Set up when the first file system registers. */
static mutex_t fs_files_lock;

static struct file** __fs_files_of(struct pcb* pcb)
{
    if(pcb == NULL) return fs_boot_files;
    if(pcb->is_process == PCB_THREAD && pcb->parent != NULL) pcb = pcb->parent;
    if(pcb->pid < 0 || pcb->pid >= MAX_NUM_OF_PCBS) return fs_boot_files;
    return fs_process_files[pcb->pid];
}

/**
 * Each process's current directory, by PID, as fs_path_normalize writes it. Empty means the root directory.
 * fs_boot_cwd is the one used before any process runs, and by the file system harness.
 */
static char fs_cwd[MAX_NUM_OF_PCBS][FS_PATH_MAX];
static char fs_boot_cwd[FS_PATH_MAX];

static char* __fs_cwd_of(struct pcb* pcb)
{
    if(pcb == NULL) return fs_boot_cwd;
    if(pcb->is_process == PCB_THREAD && pcb->parent != NULL) pcb = pcb->parent;
    if(pcb->pid < 0 || pcb->pid >= MAX_NUM_OF_PCBS) return fs_boot_cwd;
    return fs_cwd[pcb->pid];
}

/* The current process's open file for fd, or NULL */
static struct file* __fs_file(int fd)
{
    if(fd < FS_FIRST_FD || fd >= FS_MAX_PROCESS_FILES) return NULL;
    return __fs_files_of($process->current)[fd];
}

/**
 * The file system fs_get() returns and fs_open and friends use: the first one registered, behind operations that
 * normalize every path first (fs_path_normalize). Callers that use the operations directly get the same paths as
 * fs_open.
 */
static struct filesystem* fs_backend = NULL;
static struct filesystem fs_checked;
static struct filesystem* fs_current = NULL;

/**
 * Writes path into out, which holds FS_PATH_MAX bytes, in canonical form: absolute, without empty or "." parts,
 * with each ".." removing the part before it, and without a trailing slash. ".." at the root stays at the root.
 * A path that doesn't start with "/" starts from the current process's current directory.
 * @return the length, or -ERROR_INVALID_ARGUMENTS if the result wouldn't fit
 */
int fs_path_normalize(const char* path, char* out)
{
    int len = 1;
    out[0] = '/';

    /* The current directory is stored normalized, so it can be the start of out as it is */
    if(path[0] != '/'){
        const char* cwd = __fs_cwd_of($process->current);
        int cwd_len = strlen(cwd);
        if(cwd_len > 0 && cwd_len < FS_PATH_MAX){
            memcpy(out, cwd, cwd_len);
            len = cwd_len;
        }
    }

    while(*path){
        while(*path == '/') path++;
        if(*path == 0) break;

        const char* part = path;
        while(*path && *path != '/') path++;
        int n = path - part;

        if(n == 1 && part[0] == '.') continue;
        if(n == 2 && part[0] == '.' && part[1] == '.'){
            /* Back to the slash before the last part, and past it unless it's the root's */
            while(len > 1 && out[len - 1] != '/') len--;
            if(len > 1) len--;
            continue;
        }

        int sep = len > 1 ? 1 : 0;
        if(len + sep + n + 1 > FS_PATH_MAX) return -ERROR_INVALID_ARGUMENTS;
        if(sep) out[len++] = '/';
        memcpy(out + len, part, n);
        len += n;
    }

    out[len] = 0;
    return len;
}

static int __fs_checked_write(struct filesystem* fs, struct file* file, const void* buf, int size)
{
    return fs_backend->ops->write(fs_backend, file, buf, size);
}

static int __fs_checked_read(struct filesystem* fs, struct file* file, void* buf, int size)
{
    return fs_backend->ops->read(fs_backend, file, buf, size);
}

static struct file* __fs_checked_open(struct filesystem* fs, const char* path, int flags, int* error)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0){
        if(error != NULL) *error = -ERROR_INVALID_ARGUMENTS;
        return NULL;
    }
    return fs_backend->ops->open(fs_backend, normal, flags, error);
}

static int __fs_checked_close(struct filesystem* fs, struct file* file)
{
    return fs_backend->ops->close(fs_backend, file);
}

static int __fs_checked_remove(struct filesystem* fs, const char* path)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->remove(fs_backend, normal);
}

static int __fs_checked_mkdir(struct filesystem* fs, const char* path)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->mkdir(fs_backend, normal);
}

static int __fs_checked_rmdir(struct filesystem* fs, const char* path)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->rmdir(fs_backend, normal);
}

static int __fs_checked_rename(struct filesystem* fs, const char* path, const char* new_path)
{
    char normal[FS_PATH_MAX];
    char new_normal[FS_PATH_MAX];
    if(path == NULL || new_path == NULL) return -ERROR_INVALID_ARGUMENTS;
    if(fs_path_normalize(path, normal) < 0 || fs_path_normalize(new_path, new_normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->rename(fs_backend, normal, new_normal);
}

static int __fs_checked_same(struct filesystem* fs, const char* path, const char* other)
{
    char normal[FS_PATH_MAX];
    char other_normal[FS_PATH_MAX];
    if(path == NULL || other == NULL) return -ERROR_INVALID_ARGUMENTS;
    if(fs_path_normalize(path, normal) < 0 || fs_path_normalize(other, other_normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->same(fs_backend, normal, other_normal);
}

static int __fs_checked_stat(struct filesystem* fs, const char* path, struct fs_dirent* out)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->stat(fs_backend, normal, out);
}

static int __fs_checked_readdir(struct filesystem* fs, struct file* file, struct fs_dirent* out)
{
    return fs_backend->ops->readdir(fs_backend, file, out);
}

static int __fs_checked_touch(struct filesystem* fs, const char* path)
{
    char normal[FS_PATH_MAX];
    if(path == NULL || fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;
    return fs_backend->ops->touch(fs_backend, normal);
}

static struct filesystem_ops fs_checked_ops = {
    .write = __fs_checked_write,
    .read = __fs_checked_read,
    .open = __fs_checked_open,
    .close = __fs_checked_close,
    .remove = __fs_checked_remove,
    .mkdir = __fs_checked_mkdir,
    .rmdir = __fs_checked_rmdir,
    .rename = __fs_checked_rename,
    .same = __fs_checked_same,
    .stat = __fs_checked_stat,
    .readdir = __fs_checked_readdir,
    .touch = __fs_checked_touch
};

/* functions used by filesystem implementations */

/* Takes a free open file from the pool. It's in use from here, marked by nlinks, until fs_free_file. */
struct file* fs_alloc_file()
{
    struct file* file = NULL;

    acquire(&fs_files_lock);
    for(int i = 0; i < FS_MAX_FILES; i++){
        if(fs_file_table[i].nlinks == 0){
            file = &fs_file_table[i];
            memset(file, 0, sizeof(struct file));
            file->nlinks = 1;
            break;
        }
    }
    release(&fs_files_lock);

    return file;
}

int fs_free_file(struct file* file)
{
    ERR_ON_NULL(file);

    acquire(&fs_files_lock);
    file->offset = 0;
    file->flags = 0;
    file->identifier = 0;
    file->directory = 0;
    file->data = NULL;
    file->nlinks = 0;
    release(&fs_files_lock);

    return 0;
}

int fs_register(struct filesystem* fs)
{
    ERR_ON_NULL(fs);

    /* check if the filesystem has ALL ops the required functions */
    if(fs->ops->open == NULL || fs->ops->read == NULL || fs->ops->write == NULL || fs->ops->close == NULL){
        return -1;
    }

    /* find a free filesystem slot */
    for(int i = 0; i < FS_MAX_FILESYSTEMS; i++){
        if(fs_table[i] == NULL){
            fs_table[i] = fs;

            if(fs_backend == NULL){
                mutex_init(&fs_files_lock);
                fs_backend = fs;
                fs_checked = *fs;
                fs_checked.ops = &fs_checked_ops;
                fs_current = &fs_checked;
            }
            dbgprintf("Registered filesystem: %s\n", fs->name);
            return 0;
        }
    }

    /* no free filesystem slot found */
    return -1;
}

/* filesystem functions */
int fs_init()
{
    
    return 0;
}

int fs_mount(void)
{
    return fs_backend_mount();
}

int fs_format(const char* label, int reserved)
{
    return fs_backend_format(label, reserved);
}

int fs_usage(int* used_kb, int* total_kb)
{
    if(fs_backend == NULL) return -1;
    return fs_backend_usage(used_kb, total_kb);
}

int fs_unregister(struct filesystem* fs)
{
    ERR_ON_NULL(fs);

    /* find the filesystem */
    for(int i = 0; i < FS_MAX_FILESYSTEMS; i++){
        if(fs_table[i] == fs){
            fs_table[i] = NULL;
            if(fs == fs_backend){
                fs_backend = NULL;
                fs_current = NULL;
            }
            return 0;
        }
    }

    /* filesystem not found */
    return -1;
}

/**
 * @brief Returns the current filesystem.
 * 
 * @return struct filesystem* (NULL if no filesystem is available)
 */
struct filesystem* fs_get()
{
    return fs_current;
}

int fs_load_from_file(const char* file, void* buf, int size)
{
	int inode = fs_open(file, FS_FILE_FLAG_READ);
	if(inode < 0){
		dbgprintf("Error opening %s\n", file);
		return -ERROR_FILE_NOT_FOUND;
	}

	dbgprintf("Reading %s from disk\n", file);
	int read = fs_read(inode, buf, size);
	if(read < 0){
		fs_close(inode);
		return -ERROR_FILE_NOT_FOUND;
	}

	fs_close(inode);
	return read;
}

int fs_save_to_file(const char* file, void* buf, int size)
{
    int inode = fs_open(file, FS_FILE_FLAG_CREATE | FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE);
    if(inode < 0){
        dbgprintf("Error opening %s\n", file);
        return -ERROR_FILE_NOT_FOUND;
    }

    dbgprintf("Writing %s to disk\n", file);
    int written = fs_write(inode, buf, size);
    if(written < 0){
        fs_close(inode);
        return -ERROR_FILE_NOT_FOUND;
    }

    fs_close(inode);
    return written;
}

int fs_size(int fd){
    struct file* file = __fs_file(fd);
    if(file == NULL){
        return -1;
    }

    return file->size;
}

/* Takes fd out of a table of descriptors, and closes its file */
static int __fs_close(struct file** files, int fd)
{
    if(fd < FS_FIRST_FD || fd >= FS_MAX_PROCESS_FILES) return -ERROR_INVALID_ARGUMENTS;

    acquire(&fs_files_lock);
    struct file* file = files[fd];
    files[fd] = NULL;
    release(&fs_files_lock);

    if(file == NULL) return -ERROR_INVALID_ARGUMENTS;

    fs_current->ops->close(fs_current, file);
    fs_free_file(file);
    return 0;
}

int fs_close(int fd)
{
    /* check if a filesystem is available */
    if(fs_current == NULL){
        return -ERROR_NO_FILESYSTEM;
    }

    return __fs_close(__fs_files_of($process->current), fd);
}

/* Closes every file a process still has open, when it exits. A thread's files are its process's, so they stay. */
int fs_close_files_of(struct pcb* pcb)
{
    if(fs_current == NULL || pcb == NULL || pcb->is_process == PCB_THREAD) return 0;

    struct file** files = __fs_files_of(pcb);
    int closed = 0;
    for(int fd = FS_FIRST_FD; fd < FS_MAX_PROCESS_FILES; fd++){
        if(files[fd] != NULL && __fs_close(files, fd) == 0) closed++;
    }
    if(closed > 0) dbgprintf("[FS] Closed %d files %s left open\n", closed, pcb->name);
    return closed;
}

/* Opens path and gives it the lowest free descriptor in the current process's table */
int fs_open(const char* path, int flags)
{
    ERR_ON_NULL(path);

    /* check if a filesystem is available */
    if(fs_current == NULL || fs_current->ops->open == NULL){
        return -1;
    }

    /* open the file */
    int error = -ERROR_FILE_NOT_FOUND;
    struct file* file = fs_current->ops->open(fs_current, path, flags, &error);
    if(file == NULL){
        return error;
    }

    acquire(&fs_files_lock);
    struct file** files = __fs_files_of($process->current);
    int fd = -1;
    for(int i = FS_FIRST_FD; i < FS_MAX_PROCESS_FILES; i++){
        if(files[i] == NULL){
            files[i] = file;
            fd = i;
            break;
        }
    }
    release(&fs_files_lock);

    if(fd < 0){
        dbgprintf("fs_open: %s: the process has too many files open\n", path);
        fs_current->ops->close(fs_current, file);
        fs_free_file(file);
        return -ERROR_INDEX;
    }

    dbgprintf("fs_open: %s\n", path);
    return fd;
}

/**
 * Moves the file's position, from its start, its current position or its end.
 * @return the new position, or a negative value if it would be before the start
 */
int fs_seek(int fd, int offset, fs_seek_flag_t flag)
{
    /* check if a filesystem is available */
    if(fs_current == NULL){
        return -ERROR_NO_FILESYSTEM;
    }

    /* check if the file is open */
    struct file* file = __fs_file(fd);
    if(file == NULL){
        return -ERROR_INVALID_ARGUMENTS;
    }

    int position;
    switch (flag){
    case FS_SEEK_START:
        position = offset;
        break;
    case FS_SEEK_CUR:
        position = file->offset + offset;
        break;
    case FS_SEEK_END:
        position = file->size + offset;
        break;
    default:
        return -ERROR_INVALID_ARGUMENTS;
    }

    if(position < 0){
        return -ERROR_INVALID_ARGUMENTS;
    }

    file->offset = position;
    return position;
}

int fs_stat(const char* path, struct fs_dirent* out)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    if(path == NULL || out == NULL) return -ERROR_NULL_POINTER;
    return fs_current->ops->stat(fs_current, path, out);
}

/* The next entry of a directory opened with fs_open: 1, or 0 when there are no more */
int fs_readdir(int fd, struct fs_dirent* out)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    if(out == NULL) return -ERROR_NULL_POINTER;

    struct file* file = __fs_file(fd);
    if(file == NULL) return -ERROR_INVALID_ARGUMENTS;
    return fs_current->ops->readdir(fs_current, file, out);
}

int fs_mkdir(const char* path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->mkdir(fs_current, path);
}

/* Deletes a file. A directory is refused: that's fs_rmdir. */
int fs_unlink(const char* path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->remove(fs_current, path);
}

/* Deletes an empty directory */
int fs_rmdir(const char* path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->rmdir(fs_current, path);
}

/* Renames or moves a file or directory. new_path must not exist. */
int fs_rename(const char* path, const char* new_path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->rename(fs_current, path, new_path);
}

/**
 * Whether two paths name the same file or directory, though one may spell it in another case, or by its 8.3 name.
 * @return 1 or 0, or a negative error code if either doesn't exist
 */
int fs_same_entry(const char* path, const char* other)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->same(fs_current, path, other);
}

/* Sets a file's or directory's last write time to now. It doesn't create one: a missing path is an error. */
int fs_touch(const char* path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    return fs_current->ops->touch(fs_current, path);
}

/* Changes the current process's current directory to path, which must be a directory */
int fs_chdir(const char* path)
{
    if(fs_current == NULL) return -ERROR_NO_FILESYSTEM;
    if(path == NULL) return -ERROR_NULL_POINTER;

    char normal[FS_PATH_MAX];
    if(fs_path_normalize(path, normal) < 0) return -ERROR_INVALID_ARGUMENTS;

    /* A missing directory is "Directory not found", though FatFs can't tell what a missing path was meant to be */
    struct fs_dirent entry;
    int ret = fs_current->ops->stat(fs_current, normal, &entry);
    if(ret == -ERROR_FILE_NOT_FOUND) return -ERROR_PATH_NOT_FOUND;
    if(ret < 0) return ret;
    if(!(entry.attributes & FS_ATTR_DIRECTORY)) return -ERROR_NOT_DIRECTORY;

    memcpy(__fs_cwd_of($process->current), normal, strlen(normal) + 1);
    return 0;
}

/* Writes the current process's current directory into buf, which holds size bytes. @return its length. */
int fs_getcwd(char* buf, int size)
{
    if(buf == NULL) return -ERROR_NULL_POINTER;

    const char* cwd = __fs_cwd_of($process->current);
    if(cwd[0] == 0) cwd = "/";

    int len = strlen(cwd);
    if(len + 1 > size) return -ERROR_INVALID_ARGUMENTS;
    memcpy(buf, cwd, len + 1);
    return len;
}

/* Starts a new process or kernel thread in its creator's current directory. A thread shares its process's. */
void fs_inherit_cwd(struct pcb* child, struct pcb* creator)
{
    if(child == NULL || child->is_process == PCB_THREAD) return;

    char* to = __fs_cwd_of(child);
    const char* from = __fs_cwd_of(creator);
    if(to == from) return;
    memcpy(to, from, strlen(from) + 1);
}

int fs_read(int fd, void* buf, int size)
{
    ERR_ON_NULL(buf);

    /* check if a filesystem is available */
    if(fs_current == NULL){
        return -ERROR_NO_FILESYSTEM;
    }

    /* check if the file is open */
    struct file* file = __fs_file(fd);
    if(file == NULL){
        return -ERROR_INVALID_ARGUMENTS;
    }
    if(!HAS_FLAG(file->flags, FS_FILE_FLAG_READ)){
        dbgprintf("File not open for reading: %d\n", fd);
        return -ERROR_ACCESS_DENIED;
    }

    /* read the file at its position. 0 bytes means the end of the file, and a negative number an error code. */
    int ret = fs_current->ops->read(fs_current, file, buf, size);
    if(ret < 0){
        return ret;
    }

    file->offset += ret;

    dbgprintf("fs_read %d: read %d/%d\n", fd, ret, size);

    return ret;
}

int fs_write(int fd, void* buf, int size)
{
    ERR_ON_NULL(buf);

    /* check if a filesystem is available */
    if(fs_current == NULL){
        return -ERROR_NO_FILESYSTEM;
    }

    /* check if the file is open */
    struct file* file = __fs_file(fd);
    if(file == NULL){
        return -ERROR_INVALID_ARGUMENTS;
    }
    if(!(file->flags & FS_FILE_FLAG_WRITE)){
        dbgprintf("File not open for writing: %d\n", fd);
        return -ERROR_ACCESS_DENIED;
    }

    /* write the file at its position, or at its end with FS_FILE_FLAG_APPEND. Fewer bytes than asked means the disk is full. */
    int ret = fs_current->ops->write(fs_current, file, buf, size);
    if(ret < 0){
        return ret;
    }

    file->offset = (file->flags & FS_FILE_FLAG_APPEND) ? file->size : file->offset + ret;

    return ret;
}