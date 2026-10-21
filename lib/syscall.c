/**
 * @file syscall.c
 * @author Joe Bayer (joexbayer)
 * @brief Systemcall library of userspace programs
 * @version 0.1
 * @date 2022-08-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <syscall_helper.h>
#include <lib/syscall.h>
#include <stdint.h>
#include <libc.h>
#include <lib/printf.h>
#include <rtc.h>

int invoke_syscall(int i, int arg1, int arg2, int arg3)
{
    int ret;

    __asm__ volatile ("int $48" /* 48 = 0x30 */
                  : "=a" (ret)
                  : "%0" (i), "b" (arg1), "c" (arg2), "d" (arg3));
    return ret;
}

void yield()
{
    invoke_syscall(SYSCALL_YIELD, 0, 0, 0);
}

void screen_put(int x, int y, unsigned char c)
{
    invoke_syscall(SYSCALL_SCRPUT, x, y, c);
}

void print_put(unsigned char c)
{
    invoke_syscall(SYSCALL_PRTPUT, c, 0, 0);
}

void exit()
{
    invoke_syscall(SYSCALL_EXIT, 0, 0, 0);
}

void sleep(int seconds)
{
    invoke_syscall(SYSCALL_SLEEP, seconds, 0, 0);
}

void gfx_create_window(int width, int height, int flags)
{
    void* ptr = (void*) invoke_syscall(SYSCALL_GFX_WINDOW, width, height, flags);
    if (ptr == NULL) {
        /* Usually out of memory. An app without a window can't be seen or closed, so exit instead of running on. */
        printf("Not enough memory to open a window.\n");
        exit();
    }
}

int get_current_time(struct time* time)
{
    return invoke_syscall(SYSCALL_GFX_GET_TIME, (int)time, 0, 0);
}
int gfx_draw_syscall(int option, void* data, int flags)
{
    return invoke_syscall(SYSCALL_GFX_DRAW, option, (int)data, flags);
}

int gfx_set_title(const char* title)
{
    return invoke_syscall(SYSCALL_GFX_SET_TITLE, (int)title, 0, 0);
}


int gfx_set_header(const char* header)
{
    return invoke_syscall(SYSCALL_GFX_SET_HEADER, (int)header, 0, 0);
}

int open(const char* name, int flags)
{
    return invoke_syscall(SYSCALL_OPEN, (int)name, flags, 0);
}

int write(int fd, void* buffer, int size)
{
    return invoke_syscall(SYSCALL_WRITE, fd, (int)buffer, size);
}

int read(int fd, void* buffer, int size)
{
    return invoke_syscall(SYSCALL_READ, fd, (int)buffer, size);
}

int thread_create(void* entry, void* arg, int flags)
{
    return invoke_syscall(SYSCALL_CREATE_THREAD, (int)entry, (int)arg, flags);
}

void* zmalloc(int size)
{
    void* ptr = malloc(size);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

void* malloc(int size)
{
    return (void*)invoke_syscall(SYSCALL_MALLOC, size, 0, 0);
}

void free(void* ptr)
{
    invoke_syscall(SYSCALL_FREE, (int)ptr, 0, 0);
}

int fclose(int fd)
{
    return invoke_syscall(SYSCALL_CLOSE, fd, 0, 0);
}

int seek(int fd, int offset, int whence)
{
    return invoke_syscall(SYSCALL_SEEK, fd, offset, whence);
}

int stat(const char* path, struct fs_dirent* out)
{
    return invoke_syscall(SYSCALL_STAT, (int)path, (int)out, 0);
}

/* The next entry of a directory opened with open(): 1, or 0 when there are no more */
int readdir(int fd, struct fs_dirent* out)
{
    return invoke_syscall(SYSCALL_READDIR, fd, (int)out, 0);
}

int mkdir(const char* path)
{
    return invoke_syscall(SYSCALL_MKDIR, (int)path, 0, 0);
}

int unlink(const char* path)
{
    return invoke_syscall(SYSCALL_UNLINK, (int)path, 0, 0);
}

int rmdir(const char* path)
{
    return invoke_syscall(SYSCALL_RMDIR, (int)path, 0, 0);
}

int rename(const char* path, const char* new_path)
{
    return invoke_syscall(SYSCALL_RENAME, (int)path, (int)new_path, 0);
}

int system(const char* command)
{
    return invoke_syscall(SYSCALL_SYSTEM, (int)command, 0, 0);
}

#ifdef __cplusplus
}
#endif
