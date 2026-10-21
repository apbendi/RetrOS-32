/**
 * @file virtualdisk.c
 * @author Joe Bayer (joexbayer)
 * @brief Virtual disk driver.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <libc.h>
#include <ata.h>
#include <diskdev.h>
#include <stdint.h>
#include <memory.h>
#include <errors.h>
#include <virtualdisk.h>

/**
 * @brief In memory virtual harddisk
 * Mostly for testing on real hardware, where the kernel's IDE driver may not see the disk it booted from.
 * login offers to create it when no disk is attached, and formats it (#46).
 */

#define VIRTUAL_DISK_SIZE (1024*1024)

/* From the kernel heap when the disk is created. It used to be a fixed address, 7-8 MB, that other memory uses (#46). */
static char* virtual_disk_memory = NULL;

struct ide_device virtual_disk = {
    .size = VIRTUAL_DISK_SIZE/512,
    .model = "Virtual Disk"
};

/* Whether size sectors from sector from are all on the disk, so reads and writes stay inside its memory */
static int virtual_in_range(uint32_t from, uint32_t size)
{
    return virtual_disk_memory != NULL && from <= virtual_disk.size && size <= virtual_disk.size - from;
}

int virtual_read(char* buffer, uint32_t from, uint32_t size)
{
    if(!virtual_in_range(from, size)){
        return -1;
    }
    memcpy(buffer, virtual_disk_memory + (from*512), size*512);
    return size;
}

int virtual_write(char* buffer, uint32_t from, uint32_t size)
{
    if(!virtual_in_range(from, size)){
        return -1;
    }
    memcpy(virtual_disk_memory + (from*512), buffer, size*512);
    return size;
}

/* Allocates the disk's memory, blank, and attaches it as the disk. The caller formats it. */
int virtual_disk_attach()
{
    if(virtual_disk_memory == NULL){
        virtual_disk_memory = kalloc_try(VIRTUAL_DISK_SIZE);
        if(virtual_disk_memory == NULL){
            return -ERROR_ALLOC;
        }
        memset(virtual_disk_memory, 0, VIRTUAL_DISK_SIZE);
    }

    attach_disk_dev(&virtual_read, &virtual_write, &virtual_disk);
    return ERROR_OK;
}
