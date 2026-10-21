/**
 * @file string.h
 * @brief ff.c includes <string.h> for memcpy, memset, memcmp, strchr and strlen. The kernel is built without the
 * C library's headers, and its own libc.h declares these. Only FatFs's build puts this directory on the include
 * path (fs/Makefile), so nothing else picks this file up.
 */
#ifndef FATFS_STRING_H
#define FATFS_STRING_H

#include <libc.h>

#endif /* FATFS_STRING_H */
