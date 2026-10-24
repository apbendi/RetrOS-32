/**
 * @file fstool.c
 * @brief Runs RetrOS's own file system code on a disk image file, one operation per run.
 *
 * It links the objects the kernel is built from, bin/fs.o (the file layer) and bin/fatfs.o (FatFs), with host.c's
 * stand-ins for the kernel functions they call. tests/fs/run.sh uses it to check RetrOS against mtools and fsck.fat.
 *
 *   fstool IMAGE format [RESERVED]       format the image with fs_format, as login does for the virtual disk
 *   fstool IMAGE tree                    every directory and file: "D /PATH" and "F SIZE /PATH"
 *   fstool IMAGE ls PATH                 one directory: "D NAME" and "F NAME SIZE"
 *   fstool IMAGE cat PATH [CHUNK]        a file's bytes to stdout, read CHUNK bytes at a time (default 4096)
 *   fstool IMAGE write PATH              stdin as all of PATH, in one write: created or emptied first (an Editor save)
 *   fstool IMAGE writes PATH CHUNK       the same, in CHUNK-byte writes on one open file
 *   fstool IMAGE pwrite PATH OFFSET      stdin into an existing PATH at OFFSET, after a seek
 *   fstool IMAGE append PATH             stdin onto the end of an existing PATH
 *   fstool IMAGE truncate PATH           empty an existing PATH, by opening it with the truncate flag
 *   fstool IMAGE new PATH                create an empty file, as the shell's touch does where there's nothing
 *   fstool IMAGE touch PATH              fs_touch: date PATH now, by host.c's clock, 2026-01-02 3:04:06
 *   fstool IMAGE rm|mkdir|rmdir PATH     fs_unlink, fs_mkdir and fs_rmdir, which the syscalls of the same names use
 *   fstool IMAGE mv OLD NEW              fs_rename
 *   fstool IMAGE same-entry PATH OTHER   fs_same_entry: "same" or "different"
 *   fstool IMAGE stat PATH               fs_stat: "D NAME" or "F NAME SIZE"
 *   fstool IMAGE dir PATH                the same as ls
 *   fstool IMAGE pwd                     the current directory, from fs_getcwd
 *
 * Set FSTOOL_CWD to a directory to make it the current one, which relative paths start from.
 *
 * Directories are listed the way the shell and Finder list them: opened, then read with fs_readdir, which gives
 * each entry's long name, or its 8.3 name if it has none, and leaves out "." and "..". tree puts the path last, so
 * a name with spaces in it can be read back.
 *
 * A failed operation prints the error code it returned, from include/errors.h.
 *
 * Exit status: 0 done, 1 the operation failed, 2 usage, 3 the image didn't mount, 4 the operation worked but the
 * code read or wrote outside the disk, wrote past a kernel allocation or misused a lock. Those are reported on
 * stderr as "fstool: problem: ..." whatever the status. Set FSTOOL_DEBUG=1 to see the file system's own log.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <rtc.h>

/* fs_types.h, not fs.h: fs.h brings in the kernel's libc declarations */
#include <fs/fs_types.h>

int fs_mount(void);
int fs_format(const char* label, int reserved);
int fs_open(const char* path, int flags);
int fs_close(int fd);
int fs_read(int fd, void* buf, int size);
int fs_write(int fd, void* buf, int size);
int fs_seek(int fd, int offset, int flag);
int fs_stat(const char* path, struct fs_dirent* out);
int fs_readdir(int fd, struct fs_dirent* out);
int fs_mkdir(const char* path);
int fs_unlink(const char* path);
int fs_rmdir(const char* path);
int fs_rename(const char* path, const char* new_path);
int fs_same_entry(const char* path, const char* other);
int fs_touch(const char* path);
int fs_chdir(const char* path);
int fs_getcwd(char* buf, int size);

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
                    "       rm PATH | mkdir PATH | rmdir PATH | mv OLD NEW | same-entry PATH OTHER |\n"
                    "       stat PATH | dir PATH | touch PATH\n");
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

static void print_dirent(const struct fs_dirent* entry)
{
    if(entry->attributes & FS_ATTR_DIRECTORY) printf("D %s\n", entry->name);
    else printf("F %s %u\n", entry->name, entry->size);
}

/* A directory's entries through fs_readdir, with their long names */
static int dir(const char* path)
{
    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0) return failed("open", fd);

    struct fs_dirent entry;
    int ret;
    while((ret = fs_readdir(fd, &entry)) > 0) print_dirent(&entry);
    fs_close(fd);
    return ret < 0 ? failed("readdir", ret) : 0;
}

/**
 * Every directory and file under path, through fs_readdir, with full paths. The directory stays open while its
 * subdirectories are walked, one file per level.
 * @return 0, or 1 if a directory couldn't be read
 */
static int tree(const char* path, int depth)
{
    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0){
        fprintf(stderr, "fstool: can't open directory %s\n", path);
        return 1;
    }

    int status = 0;
    struct fs_dirent entry;
    int ret;
    while((ret = fs_readdir(fd, &entry)) > 0){
        char child[1024];
        snprintf(child, sizeof(child), "%s%s%s", path, path[1] == 0 ? "" : "/", entry.name);
        if(entry.attributes & FS_ATTR_DIRECTORY){
            printf("D %s\n", child);
            if(depth < 7) status |= tree(child, depth + 1);
        } else {
            printf("F %u %s\n", entry.size, child);
        }
    }
    fs_close(fd);
    if(ret < 0){
        fprintf(stderr, "fstool: can't read directory %s: %d\n", path, ret);
        status = 1;
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

    const char* cwd = getenv("FSTOOL_CWD");
    if(cwd != NULL && cwd[0] != 0){
        int ret = fs_chdir(cwd);
        if(ret != 0) return finish(failed("fs_chdir", ret));
    }

    int status;
    if(strcmp(op, "tree") == 0){
        status = tree("/", 0);
    } else if(strcmp(op, "pwd") == 0){
        char buf[512];
        int ret = fs_getcwd(buf, sizeof(buf));
        status = ret < 0 ? failed("fs_getcwd", ret) : 0;
        if(ret >= 0) printf("%s\n", buf);
    } else if(arg == NULL){
        return finish(usage());
    } else if(strcmp(op, "ls") == 0 || strcmp(op, "dir") == 0){
        status = dir(arg);
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
        /* The shell's touch opens with only the create flag */
        int fd = fs_open(arg, FS_FILE_FLAG_CREATE);
        status = fd < 0 ? failed("open", fd) : 0;
        if(fd >= 0) fs_close(fd);
    } else if(strcmp(op, "rm") == 0){
        int ret = fs_unlink(arg);
        status = ret != 0 ? failed("fs_unlink", ret) : 0;
    } else if(strcmp(op, "mkdir") == 0){
        int ret = fs_mkdir(arg);
        status = ret != 0 ? failed("fs_mkdir", ret) : 0;
    } else if(strcmp(op, "rmdir") == 0){
        int ret = fs_rmdir(arg);
        status = ret != 0 ? failed("fs_rmdir", ret) : 0;
    } else if(strcmp(op, "mv") == 0 && argc > 4){
        int ret = fs_rename(arg, argv[4]);
        status = ret != 0 ? failed("fs_rename", ret) : 0;
    } else if(strcmp(op, "same-entry") == 0 && argc > 4){
        int ret = fs_same_entry(arg, argv[4]);
        status = ret < 0 ? failed("fs_same_entry", ret) : 0;
        if(ret >= 0) printf("%s\n", ret == 1 ? "same" : "different");
    } else if(strcmp(op, "touch") == 0){
        int ret = fs_touch(arg);
        status = ret != 0 ? failed("fs_touch", ret) : 0;
    } else if(strcmp(op, "stat") == 0){
        struct fs_dirent entry;
        int ret = fs_stat(arg, &entry);
        status = ret != 0 ? failed("fs_stat", ret) : 0;
        if(ret == 0) print_dirent(&entry);
    } else {
        return finish(usage());
    }
    return finish(status);
}
