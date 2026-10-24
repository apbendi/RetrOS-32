#ifndef __FS_MODULE_H
#define __FS_MODULE_H

#include <libc.h>
#include <rtc.h>
#include <fs/fs_types.h>

#define FS_VERSION 1
#define FS_VALIDATE(fs) if(!fs || fs->version != FS_VERSION) return -1;

struct filesystem;
struct file;
struct pcb;

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
    /* Whether two paths name the same file or directory, however each spells it: 1 or 0 */
    int (*same)(struct filesystem* fs, const char* path, const char* other);
    int (*stat)(struct filesystem* fs, const char* path, struct fs_dirent* out);
    /* The next entry of an open directory, without "." and "..": 1, or 0 when there are no more */
    int (*readdir)(struct filesystem* fs, struct file* file, struct fs_dirent* out);
    /* Sets a file's or directory's last write time to now */
    int (*touch)(struct filesystem* fs, const char* path);
};

/* filesystem flags as enum */
typedef enum __fs_flags {
    FS_FLAG_INITILIZED = 1 << 0,
    FS_FLAG_UNUSED = 1 << 1,
} fs_flag_t;

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
int fs_same_entry(const char* path, const char* other);
int fs_touch(const char* path);

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
