/**
 * @file fstool.c
 * @brief Runs RetrOS's own file system code on a disk image file, one operation per run.
 *
 * It links the objects the kernel is built from, bin/fs.o (the file layer) and bin/fatfs.o (FatFs), with host.c's
 * stand-ins for the kernel functions they call. tests/fs/run.sh uses it to check RetrOS against mtools and fsck.fat.
 *
 *   fstool IMAGE format [RESERVED]       format the image with fs_format, as login does for the virtual disk
 *   fstool IMAGE tree                    every directory and file: "D /PATH" and "F /PATH SIZE"
 *   fstool IMAGE ls PATH                 one directory: "D NAME" and "F NAME SIZE"
 *   fstool IMAGE cat PATH [CHUNK]        a file's bytes to stdout, read CHUNK bytes at a time (default 4096)
 *   fstool IMAGE write PATH              stdin as all of PATH, in one write: created or emptied first (an Editor save)
 *   fstool IMAGE writes PATH CHUNK       the same, in CHUNK-byte writes on one open file
 *   fstool IMAGE pwrite PATH OFFSET      stdin into an existing PATH at OFFSET, after a seek
 *   fstool IMAGE append PATH             stdin onto the end of an existing PATH
 *   fstool IMAGE truncate PATH           empty an existing PATH, by opening it with the truncate flag
 *   fstool IMAGE new PATH                create an empty file, as the shell's "new file" does
 *   fstool IMAGE rm|mkdir|rmdir PATH     the file system's remove, mkdir and rmdir operations
 *   fstool IMAGE mv OLD NEW              the file system's rename operation
 *
 * Directories are listed the way Finder reads them: opened like a file and read one 32-byte entry at a time,
 * skipping free entries, long-name parts and the volume label. "." and ".." are left out.
 *
 * Exit status: 0 done, 1 the operation failed, 2 usage, 3 the image didn't mount, 4 the operation worked but the
 * code read or wrote outside the disk, wrote past a kernel allocation or misused a lock. Those are reported on
 * stderr as "fstool: problem: ..." whatever the status. Set FSTOOL_DEBUG=1 to see the file system's own log.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fs/fat16.h>

/* From include/fs/fs.h, which can't be included here: it brings in the kernel's libc declarations */
#define FS_FILE_FLAG_READ (1 << 0)
#define FS_FILE_FLAG_WRITE (1 << 1)
#define FS_FILE_FLAG_CREATE (1 << 3)
#define FS_FILE_FLAG_TRUNCATE (1 << 4)
#define FS_FILE_FLAG_APPEND (1 << 5)
#define FS_SEEK_START 0

struct filesystem;
struct file;
struct filesystem_ops {
    int (*write)(struct filesystem* fs, struct file* file, const void* buf, int size);
    int (*read)(struct filesystem* fs, struct file* file, void* buf, int size);
    struct file* (*open)(struct filesystem* fs, const char* path, int flags);
    int (*close)(struct filesystem* fs, struct file* file);
    int (*remove)(struct filesystem* fs, const char* path);
    int (*mkdir)(struct filesystem* fs, const char* path);
    int (*rmdir)(struct filesystem* fs, const char* path);
    int (*rename)(struct filesystem* fs, const char* path, const char* new_path);
};
struct filesystem {
    struct filesystem_ops* ops;
};

int fs_mount(void);
int fs_format(const char* label, int reserved);
int fs_open(const char* path, int flags);
int fs_close(int fd);
int fs_read(int fd, void* buf, int size);
int fs_write(int fd, void* buf, int size);
int fs_seek(int fd, int offset, int flag);
struct filesystem* fs_get(void);

/* From host.c */
extern FILE* fstool_disk;
extern long fstool_disk_blocks;
extern int fstool_problems;
void fstool_check_heap(void);

/* An operation's own failure (1, 2 or 3) wins over problems, which stderr reports either way */
static int finish(int status)
{
    fstool_check_heap();
    fflush(stdout);
    if(fstool_disk != NULL) fclose(fstool_disk);
    return status == 0 && fstool_problems > 0 ? 4 : status;
}

static int failed(const char* what, int ret)
{
    fprintf(stderr, "fstool: %s returned %d\n", what, ret);
    return 1;
}

static int usage(void)
{
    fprintf(stderr, "usage: fstool IMAGE format [RESERVED] | tree | ls PATH | cat PATH [CHUNK] | write PATH |\n"
                    "       writes PATH CHUNK | pwrite PATH OFFSET | append PATH | truncate PATH | new PATH |\n"
                    "       rm PATH | mkdir PATH | rmdir PATH | mv OLD NEW\n");
    return 2;
}

/* All of stdin */
static char* read_stdin(int* size)
{
    int capacity = 4096, length = 0;
    char* data = malloc(capacity);
    int n;
    while((n = fread(data + length, 1, capacity - length, stdin)) > 0){
        length += n;
        if(length == capacity){
            capacity *= 2;
            data = realloc(data, capacity);
        }
    }
    *size = length;
    return data;
}

/* An entry's name as text, "NAME.EXT", from its padded 8.3 fields */
static void entry_name(const struct fat16_directory_entry* entry, char* name)
{
    int len = 0;
    for(int i = 0; i < 8 && entry->filename[i] != ' '; i++) name[len++] = entry->filename[i];
    if(entry->extension[0] != ' '){
        name[len++] = '.';
        for(int i = 0; i < 3 && entry->extension[i] != ' '; i++) name[len++] = entry->extension[i];
    }
    name[len] = 0;
}

/**
 * Lists one directory, as Finder does. With recurse, prints full paths and walks into subdirectories.
 * @return 0, or 1 if a directory couldn't be opened
 */
static int list(const char* path, int recurse, int depth)
{
    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0){
        fprintf(stderr, "fstool: can't open directory %s\n", path);
        return 1;
    }

    /* Collect first, since walking into a subdirectory opens other files. One buffer per level, 8 levels. */
    static struct fat16_directory_entry entries[8][1024];
    struct fat16_directory_entry* found = entries[depth];
    int count = 0;
    struct fat16_directory_entry entry;
    while(count < 1024 && fs_read(fd, &entry, sizeof(entry)) > 0){
        found[count++] = entry;
    }
    fs_close(fd);

    int status = 0;
    for(int i = 0; i < count; i++){
        struct fat16_directory_entry* e = &found[i];
        if(e->filename[0] == 0x00 || e->filename[0] == 0xE5) continue;
        if(FAT16_IS_LONG_NAME(e) || (e->attributes & FAT16_FLAG_VOLUME_LABEL)) continue;
        if(e->filename[0] == '.') continue;

        char name[FAT16_NAME_MAX + 1];
        entry_name(e, name);
        int is_directory = e->attributes & FAT16_FLAG_SUBDIRECTORY;

        if(!recurse){
            if(is_directory) printf("D %s\n", name);
            else printf("F %s %u\n", name, e->file_size);
            continue;
        }

        char child[256];
        snprintf(child, sizeof(child), "%s%s%s", path, path[1] == 0 ? "" : "/", name);
        if(is_directory){
            printf("D %s\n", child);
            if(depth < 7) status |= list(child, 1, depth + 1);
        } else {
            printf("F %s %u\n", child, e->file_size);
        }
    }
    return status;
}

static int cat(const char* path, int chunk)
{
    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0) return failed("open", fd);

    char* buf = malloc(chunk);
    int ret;
    /* fs_read gives an error at the end of a file, never 0, so a short read is the end */
    while((ret = fs_read(fd, buf, chunk)) > 0){
        fwrite(buf, 1, ret, stdout);
        if(ret < chunk) break;
    }
    free(buf);
    fs_close(fd);
    return 0;
}

/* stdin into path: with offset -1, as all of it; with an offset, there; with append, onto the end */
static int write_file(const char* path, int chunk, int offset, int append)
{
    int size;
    char* data = read_stdin(&size);

    int flags = FS_FILE_FLAG_CREATE | FS_FILE_FLAG_READ | FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE;
    if(offset >= 0) flags = FS_FILE_FLAG_READ | FS_FILE_FLAG_WRITE;
    if(append) flags = FS_FILE_FLAG_WRITE | FS_FILE_FLAG_APPEND;
    int fd = fs_open(path, flags);
    if(fd < 0) return failed("open", fd);

    if(offset >= 0){
        int ret = fs_seek(fd, offset, FS_SEEK_START);
        if(ret < 0) return failed("seek", ret);
    }

    int status = 0;
    int done = 0;
    do {
        int n = chunk > 0 && size - done > chunk ? chunk : size - done;
        int ret = fs_write(fd, data + done, n);
        if(ret != n){
            status = failed("write", ret);
            break;
        }
        done += n;
    } while(done < size);

    fs_close(fd);
    free(data);
    return status;
}

int main(int argc, char* argv[])
{
    if(argc < 3) return usage();
    const char* image = argv[1];
    const char* op = argv[2];
    const char* arg = argc > 3 ? argv[3] : NULL;

    fstool_disk = fopen(image, "r+b");
    if(fstool_disk == NULL){
        perror(image);
        return 2;
    }
    fseek(fstool_disk, 0, SEEK_END);
    fstool_disk_blocks = ftell(fstool_disk) / 512;

    if(strcmp(op, "format") == 0){
        int ret = fs_format("RETROS", arg != NULL ? atoi(arg) : 4);
        return finish(ret == 0 ? 0 : failed("fs_format", ret));
    }

    if(fs_mount() != 0){
        fprintf(stderr, "fstool: the image didn't mount\n");
        return finish(3);
    }

    struct filesystem* fs = fs_get();
    int status;
    if(strcmp(op, "tree") == 0){
        status = list("/", 1, 0);
    } else if(arg == NULL){
        return finish(usage());
    } else if(strcmp(op, "ls") == 0){
        status = list(arg, 0, 0);
    } else if(strcmp(op, "cat") == 0){
        status = cat(arg, argc > 4 ? atoi(argv[4]) : 4096);
    } else if(strcmp(op, "write") == 0){
        status = write_file(arg, 0, -1, 0);
    } else if(strcmp(op, "writes") == 0 && argc > 4){
        status = write_file(arg, atoi(argv[4]), -1, 0);
    } else if(strcmp(op, "pwrite") == 0 && argc > 4){
        status = write_file(arg, 0, atoi(argv[4]), 0);
    } else if(strcmp(op, "append") == 0){
        status = write_file(arg, 0, -1, 1);
    } else if(strcmp(op, "truncate") == 0){
        int fd = fs_open(arg, FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE);
        status = fd < 0 ? failed("open", fd) : 0;
        if(fd >= 0) fs_close(fd);
    } else if(strcmp(op, "new") == 0){
        /* The shell's "new file" opens with only the create flag */
        int fd = fs_open(arg, FS_FILE_FLAG_CREATE);
        status = fd < 0 ? failed("open", fd) : 0;
        if(fd >= 0) fs_close(fd);
    } else if(strcmp(op, "rm") == 0){
        int ret = fs->ops->remove(fs, arg);
        status = ret != 0 ? failed("remove", ret) : 0;
    } else if(strcmp(op, "mkdir") == 0){
        int ret = fs->ops->mkdir(fs, arg);
        status = ret != 0 ? failed("mkdir", ret) : 0;
    } else if(strcmp(op, "rmdir") == 0){
        int ret = fs->ops->rmdir(fs, arg);
        status = ret != 0 ? failed("rmdir", ret) : 0;
    } else if(strcmp(op, "mv") == 0 && argc > 4){
        int ret = fs->ops->rename(fs, arg, argv[4]);
        status = ret != 0 ? failed("rename", ret) : 0;
    } else {
        return finish(usage());
    }
    return finish(status);
}
