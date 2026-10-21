/**
 * @file build.c
 * @author Joe Bayer (joexbayer)
 * @brief Build RetrOS images.
 * @version 0.2
 * @date 2024-01-10
 *
 * @copyright Copyright (c) 2024
 *
 * The image is a FAT16 volume with no partition table, made by mkfs.fat, with the bootloader and the kernel in its
 * reserved sectors: the bootloader in sectors 0 to 3, around the boot sector's FAT fields, and the kernel from
 * sector 4. The bootloader reads the kernel's size from the reserved sector count (boot sector offset 14), so that
 * count is exactly the kernel's sectors plus 4. make img's sync step then copies rootfs/ onto the volume.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE 512
#define BOOTBLOCK_BLOCKS 4
#define BOOTBLOCK_SIZE (BLOCK_SIZE * BOOTBLOCK_BLOCKS)

/* The boot sector's FAT fields, from the OEM name to the end of the FAT16 extended fields */
#define BPB_START 3
#define BPB_END 62

/* The partition table in the boot sector, which the bootloader leaves room for */
#define PARTITION_TABLE 446
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_ACTIVE 0x80
#define PARTITION_FAT16_LBA 0x0E

/* TODO: as argv */
static const int IMG_SIZE = 32*1024*1024;

static int read_file(const char* path, char** data, long* size)
{
    FILE* file = fopen(path, "rb");
    if(file == NULL) return -1;

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Whole blocks, zeroed, so a last partial block is padded with zeros */
    long blocks = (*size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    *data = calloc(blocks > 0 ? blocks : 1, BLOCK_SIZE);
    if(*data == NULL || (long)fread(*data, 1, *size, file) != *size){
        fclose(file);
        return -2;
    }

    fclose(file);
    return 0;
}

/* Formats image with mkfs.fat, reserving reserved sectors, or mkfs.fat's default if reserved is 0 */
static int format(const char* image, int reserved)
{
    char command[256];
    char reserved_opt[32] = "";

    if(reserved > 0) snprintf(reserved_opt, sizeof(reserved_opt), "-R %d", reserved);
    remove(image);
    snprintf(command, sizeof(command), "mkfs.fat -F 16 %s -n RETROS32 -C %s %d >/dev/null", reserved_opt, image, IMG_SIZE / 1024);

    return system(command) == 0 ? 0 : -1;
}

/**
 * Writes the bootloader into sectors 0 to 3, keeping the FAT fields mkfs.fat wrote into the boot sector, and a
 * bootable partition entry covering the disk, as some BIOSes want to see before they boot from it.
 */
static int write_bootloader(FILE* image)
{
    char* bootblock;
    long size;
    if(read_file("bin/bootblock", &bootblock, &size) < 0 || size != BOOTBLOCK_SIZE){
        return -1;
    }

    unsigned char boot_sector[BLOCK_SIZE];
    fseek(image, 0, SEEK_SET);
    if(fread(boot_sector, 1, BLOCK_SIZE, image) != BLOCK_SIZE){
        free(bootblock);
        return -2;
    }

    memcpy(bootblock + BPB_START, boot_sector + BPB_START, BPB_END - BPB_START);

    unsigned char* entry = (unsigned char*)bootblock + PARTITION_TABLE;
    uint32_t sectors = IMG_SIZE / BLOCK_SIZE;
    memset(entry, 0, 4 * PARTITION_ENTRY_SIZE);
    entry[0] = PARTITION_ACTIVE;
    entry[4] = PARTITION_FAT16_LBA;
    /* LBA start 0, then the sector count, both little-endian */
    memcpy(entry + 12, &sectors, sizeof(sectors));

    fseek(image, 0, SEEK_SET);
    size_t written = fwrite(bootblock, 1, BOOTBLOCK_SIZE, image);
    free(bootblock);
    return written == BOOTBLOCK_SIZE ? 0 : -3;
}

static int write_kernel(FILE* image, char* kernel, long size)
{
    long blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    fseek(image, BOOTBLOCK_SIZE, SEEK_SET);
    return (long)fwrite(kernel, BLOCK_SIZE, blocks, image) == blocks ? 0 : -1;
}

/**
 * @brief build - Create RetrOS images
 *
 * Usage: ./build [-opts]
 * Options
 *  -d Disk only (no kernel or bootloader)
 *  -r Build release version
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return int Exit status
 */
int main(int argc, char const *argv[]) {

    const char* outname;
    int disk_only = 0;
    int release = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            disk_only = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            release = 1;
        }
    }

    if(release){
        outname = "RetrOS-32.img";
    } else {
        outname = "RetrOS-32-debug.img";
    }

    if(disk_only){
        outname = "RetrOS-32-disk.img";
        printf("Building disk image only.\n");
        if(format(outname, 0) < 0){
            printf("mkfs.fat failed.\n");
            return -1;
        }
        return 0;
    }

    printf("Building disk image with kernel and bootloader.\n");

    char* kernel;
    long kernel_size;
    if(read_file("bin/kernelout", &kernel, &kernel_size) < 0){
        printf("Unable to load kernel.\n");
        return -1;
    }
    int kernel_blocks = (kernel_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    printf("Kernel size: %ld\n", kernel_size);

    /* The bootloader reads the kernel's size from the reserved sector count */
    if(format(outname, kernel_blocks + BOOTBLOCK_BLOCKS) < 0){
        printf("mkfs.fat failed.\n");
        return -1;
    }

    FILE* image = fopen(outname, "r+b");
    if(image == NULL){
        printf("Unable to open %s.\n", outname);
        return -1;
    }

    int ret = write_bootloader(image);
    if(ret < 0){
        printf("Unable to write bootloader: %d\n", ret);
        return -1;
    }

    if(write_kernel(image, kernel, kernel_size) < 0){
        printf("Unable to write kernel.\n");
        return -1;
    }

    fclose(image);
    free(kernel);
    printf("Formatted %s with mkfs.fat: %d reserved sectors for the bootloader and kernel.\n", outname, kernel_blocks + BOOTBLOCK_BLOCKS);

    return 0;
}
