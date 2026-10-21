/**
 * @file host.c
 * @brief Host stand-ins for the kernel functions RetrOS's file system code calls, so fstool can run the real
 * bin/fs.o and bin/fatfs.o, compiled exactly as for the kernel, against a disk image file.
 *
 * Besides standing in, these watch for mistakes the kernel wouldn't notice: a read or write outside the disk,
 * a write past the end of a kalloc'd buffer, and releasing a lock that isn't held. Each prints a line starting
 * "fstool: problem: " to stderr and is counted. fstool exits with status 4 if any happened during an operation
 * that otherwise succeeded.
 *
 * No kernel headers here: they declare libc functions with the kernel's own signatures. The structs the kernel
 * code shares with these functions are declared to match include/rtc.h, include/kutils.h and include/pcb.h.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* The disk: fstool opens the image and sets these */
FILE* fstool_disk = NULL;
long fstool_disk_blocks = 0;

/* Mistakes seen so far */
int fstool_problems = 0;

static void problem(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "fstool: problem: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fstool_problems++;
}

/* --- Disk ------------------------------------------------------------------------------------------------- */

int disk_attached(void)
{
    return fstool_disk != NULL;
}

int disk_size(void)
{
    return (int)(fstool_disk_blocks * 512);
}

/* Out-of-range blocks are refused rather than read as zeros or written past the end, which would grow the file */
int read_block(char* buf, int block)
{
    if(block < 0 || block >= fstool_disk_blocks){
        problem("read outside the disk: block %d of %ld", block, fstool_disk_blocks);
        for(int i = 0; i < 512; i++) buf[i] = 0;
        return -1;
    }
    fseek(fstool_disk, (long)block * 512, SEEK_SET);
    if(fread(buf, 1, 512, fstool_disk) != 512){
        problem("short read of block %d", block);
        return -1;
    }
    return 1;
}

int write_block(char* buf, int block)
{
    if(block < 0 || block >= fstool_disk_blocks){
        problem("write outside the disk: block %d of %ld", block, fstool_disk_blocks);
        return -1;
    }
    fseek(fstool_disk, (long)block * 512, SEEK_SET);
    if(fwrite(buf, 1, 512, fstool_disk) != 512){
        problem("short write of block %d", block);
        return -1;
    }
    return 1;
}

int write_block_offset(char* usr_buf, int size, int offset, int block)
{
    char buf[512];
    if(offset < 0 || size < 0 || offset + size > 512){
        problem("partial block write crosses the block's end: %d bytes at offset %d of block %d", size, offset, block);
        return -1;
    }
    if(read_block(buf, block) < 0) return -1;
    for(int i = 0; i < size; i++) buf[offset + i] = usr_buf[i];
    return write_block(buf, block);
}

int read_block_offset(char* usr_buf, int size, int offset, int block)
{
    char buf[512];
    if(offset < 0 || size < 0 || offset + size > 512){
        problem("partial block read crosses the block's end: %d bytes at offset %d of block %d", size, offset, block);
        return -1;
    }
    if(read_block(buf, block) < 0) return -1;
    for(int i = 0; i < size; i++) usr_buf[i] = buf[offset + i];
    return 1;
}

/* --- Memory ----------------------------------------------------------------------------------------------- */

/**
 * Each allocation gets this much zeroed space after it, and it must still be zero when freed or when fstool
 * ends. Zeros, because that's what the kernel heap often holds, so code that runs past a buffer behaves as it
 * would there. 128 KB covers the FAT search running to entry 65,535 past a small disk's FAT.
 */
#define SLACK (128 * 1024)
#define MAX_ALLOCATIONS 4096

static struct {
    unsigned char* ptr;
    int size;
} allocations[MAX_ALLOCATIONS];

static void check_slack(int i)
{
    for(int j = 0; j < SLACK; j++){
        if(allocations[i].ptr[allocations[i].size + j] != 0){
            problem("write past the end of a %d-byte kernel allocation, %d bytes past", allocations[i].size, j);
            return;
        }
    }
}

void* kalloc(int size)
{
    unsigned char* ptr = calloc(1, (size_t)size + SLACK);
    if(ptr == NULL) return NULL;
    for(int i = 0; i < MAX_ALLOCATIONS; i++){
        if(allocations[i].ptr == NULL){
            allocations[i].ptr = ptr;
            allocations[i].size = size;
            break;
        }
    }
    return ptr;
}

void* kalloc_try(int size)
{
    return kalloc(size);
}

/* kalloc's memory is already zeroed */
void* kcalloc(int size)
{
    return kalloc(size);
}

void kfree(void* ptr)
{
    for(int i = 0; i < MAX_ALLOCATIONS; i++){
        if(ptr != NULL && allocations[i].ptr == ptr){
            check_slack(i);
            allocations[i].ptr = NULL;
            break;
        }
    }
    free(ptr);
}

/* Checks what's still allocated. fstool calls it before exiting. */
void fstool_check_heap(void)
{
    for(int i = 0; i < MAX_ALLOCATIONS; i++){
        if(allocations[i].ptr != NULL) check_slack(i);
    }
}

/* --- Locks ------------------------------------------------------------------------------------------------ */

/* One thread, so locks only need their state tracked, to catch releasing one that isn't held (#38) */
#define MAX_LOCKS 64
static struct {
    void* lock;
    int held;
} locks[MAX_LOCKS];

static int lock_index(void* lock)
{
    for(int i = 0; i < MAX_LOCKS; i++){
        if(locks[i].lock == lock) return i;
    }
    for(int i = 0; i < MAX_LOCKS; i++){
        if(locks[i].lock == NULL){
            locks[i].lock = lock;
            return i;
        }
    }
    return 0;
}

void mutex_init(void* lock)
{
    locks[lock_index(lock)].held = 0;
}

void acquire(void* lock)
{
    int i = lock_index(lock);
    if(locks[i].held) problem("acquire of a lock that's already held");
    locks[i].held = 1;
}

void release(void* lock)
{
    int i = lock_index(lock);
    if(!locks[i].held) problem("release of a lock that isn't held");
    locks[i].held = 0;
}

/* --- Everything else -------------------------------------------------------------------------------------- */

/* Matches include/rtc.h */
struct time {
    uint8_t centuary;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

/* A fixed time, so the same steps write the same bytes: 2 January 2026, 03:04:06 */
int get_current_time(struct time* t)
{
    t->centuary = 20;
    t->year = 26;
    t->month = 1;
    t->day = 2;
    t->hour = 3;
    t->minute = 4;
    t->second = 6;
    return 0;
}

/* dbgprintf and warningf. Shown with FSTOOL_DEBUG=1. */
uint32_t serial_printf(char* fmt, ...)
{
    static int debug = -1;
    if(debug < 0) debug = getenv("FSTOOL_DEBUG") != NULL;
    if(debug){
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    return 0;
}

char* error_get_string(int err)
{
    (void)err;
    return "error";
}

/* Matches include/kutils.h. Only used for the list command's output, which fstool doesn't show. */
struct unit {
    char* unit;
    int size;
};

struct unit calculate_size_unit(int bytes)
{
    struct unit u = { "b", bytes };
    return u;
}

int time_format_date(char* buf, int size, const struct time* t, int style)
{
    (void)t; (void)style;
    if(size > 0) buf[0] = 0;
    return 0;
}

int time_format_time(char* buf, int size, const struct time* t, int seconds)
{
    (void)t; (void)seconds;
    if(size > 0) buf[0] = 0;
    return 0;
}

/**
 * $process->current is NULL, so the kernel's twritef, which writes to the current process's terminal, writes
 * nothing. Matches include/pcb.h's struct process.
 */
struct fstool_process {
    void* current;
};
static struct fstool_process fstool_process = { NULL };
struct fstool_process* $process = &fstool_process;
