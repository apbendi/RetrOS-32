#ifndef __DISK_H
#define  __DISK_H

#include <ata.h>

void attach_disk_dev(
    int (*read)(char* buffer, uint32_t from, uint32_t size), 
    int (*write)(char* buffer, uint32_t from, uint32_t size),
    int (*flush)(void),
    struct ide_device* dev
);

struct diskdev {
    int32_t (*read)(char* buffer, uint32_t from, uint32_t size);
    int32_t (*write)(char* buffer, uint32_t from, uint32_t size);
    /* Puts what's been written onto the disk itself, past any cache: 0, or -1. NULL for a disk with no cache. */
    int (*flush)(void);

    uint8_t attached;

    struct ide_device* dev;
};

int disk_attached();

char* disk_name();

struct diskdev* disk_device_get();

int write_block(void* buf, int block);
int write_block_offset(void* usr_buf, int size, int offset, int block);

int read_block(void* buf, int block);
int read_block_offset(void* usr_buf, int size, int offset, int block);

int disk_flush(void);

int disk_size();

#endif