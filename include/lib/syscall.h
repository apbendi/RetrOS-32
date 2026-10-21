#ifndef __SYSCALL_H
#define __SYSCALL_H

#ifdef __cplusplus
extern "C"
{
#endif

int invoke_syscall(int i, int arg1, int arg2, int arg3);

void screen_put(int x, int y, unsigned char c);
void print_put(unsigned char c);
void exit();
void sleep(int seconds);

void gfx_create_window(int width, int height, int flags);

int gfx_draw_syscall(int option, void* data, int flags);
int gfx_set_title(const char* title);
int gfx_set_header(const char* header);

int open(const char* name, int flags);
int write(int fd, void* buffer, int size);
int read(int fd, void* buffer, int size);
int fclose(int fd);

/* Files and directories. struct fs_dirent and the seek modes are in fs/fs.h. Each returns a negative error code
 * from errors.h on failure. */
struct fs_dirent;
int seek(int fd, int offset, int whence);
int stat(const char* path, struct fs_dirent* out);
int readdir(int fd, struct fs_dirent* out);
int mkdir(const char* path);
int unlink(const char* path);
int rmdir(const char* path);
int rename(const char* path, const char* new_path);
/* The current directory, which relative paths start from. A program starts in the one it was started from. */
int chdir(const char* path);
int getcwd(char* buf, int size);

void* zmalloc(int size);
void* malloc(int size);
void free(void* ptr);

int thread_create(void* entry, void* arg, int flags);
void yield();

int system(const char* command);

#ifdef __cplusplus
}
#endif

#endif /* __SYSCALL_H */
