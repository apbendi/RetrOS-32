#ifndef __FS_MODULE_H
#define __FS_MODULE_H

#include <libc.h>
#include <rtc.h>

#define FS_VERSION 1
#define FS_VALIDATE(fs) if(!fs || fs->version != FS_VERSION) return -1;

/* The longest path, including its terminating 0. Paths are checked against it before the file system sees them. */
#define FS_PATH_MAX 512

/* Open files per process, as file descriptors. 0 to 2 are never used. */
#define FS_MAX_PROCESS_FILES 32

/* The longest name of a file or directory, not counting its terminating 0 */
#define FS_NAME_MAX 255

/* A file's or directory's attributes, as FAT stores them */
#define FS_ATTR_READ_ONLY 0x01
#define FS_ATTR_HIDDEN 0x02
#define FS_ATTR_SYSTEM 0x04
#define FS_ATTR_DIRECTORY 0x10
#define FS_ATTR_ARCHIVE 0x20

/* What fs_stat and fs_readdir report about a file or directory */
struct fs_dirent {
    char name[FS_NAME_MAX + 1];
    unsigned int size;
    unsigned char attributes;
    /* When it was last written. A year of 0 means it has no date. */
    struct time written;
};

struct filesystem;
struct file;
struct pcb;

/* file flags enum */
typedef enum __fs_file_flags {
    FS_FILE_FLAG_READ = 1 << 0,
    FS_FILE_FLAG_WRITE = 1 << 1,
    FS_FILE_FLAG_EXECUTE = 1 << 2,
    /* Create the file if it doesn't exist. An existing file is kept as it is. */
    FS_FILE_FLAG_CREATE = 1 << 3,
    /* Empty the file when it's opened, like O_TRUNC */
    FS_FILE_FLAG_TRUNCATE = 1 << 4,
    /* Every write goes to the end of the file, like O_APPEND */
    FS_FILE_FLAG_APPEND = 1 << 5
} fs_file_flag_t;

struct file {
    /* flags: fs_file_flag_t*/
    char flags;
    int offset;
    /* amount of owners */
    int nlinks;
    int identifier;
    int directory;
    int size;
    /* The file system's own state for the open file or directory, if it keeps any */
    void* data;
};

/* none of the functions can ever be NULL */
struct filesystem_ops {
    /* basic functionality */
    int (*write)(struct filesystem* fs, struct file* file, const void* buf, int size);
    int (*read)(struct filesystem* fs, struct file* file, void* buf, int size);
    /* On failure, sets *error, if error isn't NULL, to a negative error code */
    struct file* (*open)(struct filesystem* fs, const char* path, int flags, int* error);
    int (*close)(struct filesystem* fs, struct file* file);

    /* extended functionality */
    int (*remove)(struct filesystem* fs, const char* path);
    int (*mkdir)(struct filesystem* fs, const char* path);
    int (*rmdir)(struct filesystem* fs, const char* path);
    int (*rename)(struct filesystem* fs, const char* path, const char* new_path);
    int (*stat)(struct filesystem* fs, const char* path, struct fs_dirent* out);
    /* The next entry of an open directory, without "." and "..": 1, or 0 when there are no more */
    int (*readdir)(struct filesystem* fs, struct file* file, struct fs_dirent* out);
};

/* filesystem flags as enum */
typedef enum __fs_flags {
    FS_FLAG_INITILIZED = 1 << 0,
    FS_FLAG_UNUSED = 1 << 1,
} fs_flag_t;

typedef enum __fs_seek_flags {
    FS_SEEK_START = 0,
    FS_SEEK_CUR = 1,
    FS_SEEK_END = 2
} fs_seek_flag_t;

struct filesystem {
    struct filesystem_ops* ops;
    unsigned char flags;
    char name[32];
    int version;
    int type;
};

/* filesystem functions */
int fs_register(struct filesystem* fs);
int fs_unregister(struct filesystem* fs);

int fs_load_from_file(const char* file, void* buf, int size);
int fs_save_to_file(const char* file, void* buf, int size);

struct file* fs_alloc_file();

int fs_init();

int fs_seek(int fd, int offset, fs_seek_flag_t flag);

int fs_open(const char* path, int flags);
int fs_close(int fd);
int fs_read(int fd, void* buf, int size);
int fs_size(int fd);
int fs_write(int fd, void* buf, int size);
int fs_close_files_of(struct pcb* pcb);
struct filesystem* fs_get();

/* Directories and file information. Each returns a negative error code from errors.h on failure. */
int fs_stat(const char* path, struct fs_dirent* out);
int fs_readdir(int fd, struct fs_dirent* out);
int fs_mkdir(const char* path);
int fs_unlink(const char* path);
int fs_rmdir(const char* path);
int fs_rename(const char* path, const char* new_path);

/* The current directory, which relative paths start from. Each process has its own, and a thread uses its
 * process's. A new process or kernel thread starts in its creator's (fs_inherit_cwd). */
int fs_chdir(const char* path);
int fs_getcwd(char* buf, int size);
void fs_inherit_cwd(struct pcb* child, struct pcb* creator);

int fs_path_normalize(const char* path, char* out);

/* Mount the disk's file system, or format the disk, with whichever FAT code the kernel is built with */
int fs_mount(void);
int fs_format(const char* label, int reserved);
/* How much of the mounted file system is used, and its size, in KB */
int fs_usage(int* used_kb, int* total_kb);

/* The FAT code provides these. reserved is the number of blocks before the file system; FatFs ignores it. */
int fs_backend_mount(void);
int fs_backend_format(const char* label, int reserved);
int fs_backend_usage(int* used_kb, int* total_kb);


#endif /* !__FS_MODULE_H */
