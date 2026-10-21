/**
 * @file partition.c
 * @author Joe Bayer (joexbayer)
 * @brief Partition table parsing.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <mbr.h>
#include <fs/fs.h>
#include <kutils.h>
#include <libc.h>
#include <serial.h>
#include <diskdev.h>

#define BOOT_BLOCK 0
#define BOOT_BLOCK_MEMORY 0x7c00

const char* mbr_partition_type_string(mbr_partition_type_t type)
{
    switch (type) {
        case MBR_TYPE_EMPTY: return "Empty or unused partition";
        case MBR_TYPE_FAT12: return "FAT12 filesystem";
        case MBR_TYPE_XENIX_ROOT: return "XENIX root filesystem";
        case MBR_TYPE_XENIX_USR: return "XENIX /usr filesystem";
        case MBR_TYPE_FAT16_LT32M: return "FAT16 filesystem with less than 32MB";
        case MBR_TYPE_EXTENDED: return "Extended partition";
        case MBR_TYPE_FAT16_GT32M: return "FAT16 filesystem with more than 32MB";
        case MBR_TYPE_NTFS: return "NTFS or exFAT filesystem";
        case MBR_TYPE_FAT32_CHS: return "FAT32 filesystem (CHS addressing)";
        case MBR_TYPE_FAT32_LBA: return "FAT32 filesystem (LBA addressing)";
        case MBR_TYPE_FAT16_LBA: return "FAT16 filesystem";
        case MBR_TYPE_EXTENDED_LBA: return "Extended partition using LBA";
        case MBR_TYPE_LINUX: return "Linux native partition";
        default: return "Unknown partition type";
    }
}

static struct mbr mbr = {0};

/**
 * @brief Logs the partition table. Mounting doesn't depend on it: fs_mount finds the volume either way. A disk
 * formatted without a partition table has boot code where the table would be, so its entries are meaningless.
 * 
 * @return int 
 */
int mbr_partitions_parse()
{
    dbgprintf("Partitions : boot.  type.   lbs_start.    sectors  \n");
    for(int i = 0; i < 4; i++){
        if(mbr.part[i].type == 0) continue;

        dbgprintf("Partition %d:   %s    %s,       %d,         %d\n", i, mbr.part[i].status == MBR_STATUS_ACTIVE ? "*": " ",  mbr_partition_type_string(mbr.part[i].type), mbr.part[i].lba_start, mbr.part[i].num_sectors);
    }

    return 0;
}

struct mbr* mbr_get()
{
    return &mbr;
}

/**
 * @brief Loads the MBR from the boot block.
 * 
 * @return int 
 */
int mbr_partition_load()
{

    read_block(&mbr, BOOT_BLOCK);

    mbr_partitions_parse();

    return 0;

}