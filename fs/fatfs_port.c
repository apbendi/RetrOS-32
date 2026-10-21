/**
 * @file fatfs_port.c
 * @brief The functions FatFs (fs/fatfs) calls into RetrOS: sector I/O on the attached disk, the time for file
 * dates, memory for long file names, and the locks that let several threads use the file system at once.
 *
 * FatFs ships samples of these as diskio.c and ffsystem.c. RetrOS has one disk, so the drive number is always 0.
 */
#include <diskdev.h>
#include <rtc.h>
#include <memory.h>
#include <sync.h>

#include "fatfs/ff.h"
#include "fatfs/diskio.h"

#define SECTOR_SIZE 512

/* --- Disk ----------------------------------------------------------------------------------------------------- */

DSTATUS disk_status(BYTE pdrv)
{
    return pdrv == 0 && disk_attached() ? 0 : STA_NOINIT;
}

/* The disk driver is ready as soon as it's attached, so there's nothing to set up */
DSTATUS disk_initialize(BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if(disk_status(pdrv) != 0) return RES_NOTRDY;

    for(UINT i = 0; i < count; i++){
        if(read_block(buff + i * SECTOR_SIZE, sector + i) < 0) return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if(disk_status(pdrv) != 0) return RES_NOTRDY;

    for(UINT i = 0; i < count; i++){
        if(write_block((void*)(buff + i * SECTOR_SIZE), sector + i) < 0) return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    if(disk_status(pdrv) != 0) return RES_NOTRDY;

    switch(cmd){
    case CTRL_SYNC:
        /* Writes go straight to the disk */
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t*)buff = disk_size() / SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        /* The erase block size, in sectors, which f_mkfs aligns the data area to. 1 means none. */
        *(DWORD*)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

/* --- Time ----------------------------------------------------------------------------------------------------- */

/**
 * The time for file dates, packed as FAT stores it: years since 1980, month, day, hour, minute, and seconds / 2.
 * FAT keeps local time, and that's what the RTC holds (#20 covers keeping it in UTC).
 */
DWORD get_fattime(void)
{
    struct time now;
    get_current_time(&now);

    int year = now.centuary * 100 + now.year;
    return ((DWORD)(year - 1980) << 25)
         | ((DWORD)now.month << 21)
         | ((DWORD)now.day << 16)
         | ((DWORD)now.hour << 11)
         | ((DWORD)now.minute << 5)
         | ((DWORD)now.second / 2);
}

/* --- Memory --------------------------------------------------------------------------------------------------- */

/* FatFs's working buffer for long file names. It handles NULL, so this mustn't use kalloc, which panics instead. */
void* ff_memalloc(UINT msize)
{
    return kalloc_try(msize);
}

void ff_memfree(void* mblock)
{
    kfree(mblock);
}

/* --- Locks ---------------------------------------------------------------------------------------------------- */

/* One per volume, and one more FatFs uses for its own tables when file locking is on */
static mutex_t fatfs_locks[FF_VOLUMES + 1];

int ff_mutex_create(int vol)
{
    mutex_init(&fatfs_locks[vol]);
    return 1;
}

void ff_mutex_delete(int vol)
{
    (void)vol;
}

/* acquire waits as long as it takes, so FatFs's timeout never runs out */
int ff_mutex_take(int vol)
{
    acquire(&fatfs_locks[vol]);
    return 1;
}

void ff_mutex_give(int vol)
{
    release(&fatfs_locks[vol]);
}
