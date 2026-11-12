/**
 * Host stand-in for include/lib/syscall.h: the system calls the Editor makes, with the real ones' signatures. host.cpp
 * implements them over an in-memory file system and clipboard. exit() throws EditorExit, so a test gets control back
 * from Quit.
 */
#ifndef EDITOR_TEST_SYSCALL_H
#define EDITOR_TEST_SYSCALL_H
#include <cstddef>
#include <cstdint>
#include <clipboard.h>
#include <fs/fs.h>

struct EditorExit {};

void exit();
void sleep(int ms);
int open(const char* name, int flags);
int write(int fd, void* buffer, int size);
int read(int fd, void* buffer, int size);
int fclose(int fd);
int stat(const char* path, struct fs_dirent* out);
int getcwd(char* buf, int size);
#endif
