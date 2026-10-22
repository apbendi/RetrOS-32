#ifndef __FS_TYPES_H
#define __FS_TYPES_H

/**
 * What the file system's system calls take and return: limits, flags, attributes and struct fs_dirent.
 * It includes nothing but rtc.h, so programs built on the standard C library (libc/) can use it next to standard
 * headers. fs.h includes it for the kernel.
 */
#include <rtc.h>

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

typedef enum __fs_seek_flags {
    FS_SEEK_START = 0,
    FS_SEEK_CUR = 1,
    FS_SEEK_END = 2
} fs_seek_flag_t;

#endif /* __FS_TYPES_H */
