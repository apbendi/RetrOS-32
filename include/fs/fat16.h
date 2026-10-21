#ifndef _FAT16_H
#define _FAT16_H

#include <stdint.h>

/**
 * The 32-byte FAT directory entry. The FAT code is FatFs now (fs/fatfs), but reading an open directory still
 * gives entries in this form, which Finder, the Editor's tree view and the file system harness parse (#102).
 */

typedef enum {
    FAT16_FLAG_READ_ONLY = 1 << 0,         /* Indicates that the file is read-only */
    FAT16_FLAG_HIDDEN = 1 << 1,            /* Indicates a hidden file */
    FAT16_FLAG_SYSTEM = 1 << 2,            /* Indicates a system file */
    FAT16_FLAG_VOLUME_LABEL = 1 << 3,      /* Indicates a special entry containing the volume label */
    FAT16_FLAG_SUBDIRECTORY = 1 << 4,      /* Indicates that the entry describes a subdirectory */
    FAT16_FLAG_ARCHIVE = 1 << 5,           /* Archive flag */
    FAT16_FLAG_UNUSED1 = 1 << 6,           /* Not used; must be set to 0 */
    FAT16_FLAG_UNUSED2 = 1 << 7            /* Not used; must be set to 0 */
} fat16_flag_t;

/* Long-file-name entries have exactly these four flags */
#define FAT16_LONG_NAME (FAT16_FLAG_READ_ONLY | FAT16_FLAG_HIDDEN | FAT16_FLAG_SYSTEM | FAT16_FLAG_VOLUME_LABEL)
#define FAT16_IS_LONG_NAME(entry) (((entry)->attributes & 0x3F) == FAT16_LONG_NAME)

/* The longest name "FILENAME.EXT", not counting the terminating 0 */
#define FAT16_NAME_MAX 12

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
struct fat16_directory_entry {
    union {
        struct {
            uint8_t filename[8];
            uint8_t extension[3];
        };
        uint8_t full_name[11];
    };
    uint8_t attributes;                 /* 1 byte - File attributes */
    uint8_t reserved[10];               /* 10 bytes - Reserved for use by Windows NT */
    uint16_t write_time;                /* 2 bytes - Time of the last write */
    uint16_t write_date;                /* 2 bytes - Date of the last write */
    uint16_t first_cluster;             /* 2 bytes - First cluster in the file's FAT chain */
    uint32_t file_size;                 /* 4 bytes - File size in bytes */
} __attribute__((packed));
#pragma GCC diagnostic pop

#endif
