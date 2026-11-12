/* Host stand-in for include/libc.h: the host's C library, and csprintf, which drawStatusLine formats with */
#ifndef EDITOR_TEST_LIBC_H
#define EDITOR_TEST_LIBC_H
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static inline int32_t csprintf(char* buffer, const char* fmt, va_list args) { return vsprintf(buffer, fmt, args); }
#endif
