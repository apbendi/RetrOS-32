/**
 * @file tar.c
 * @author Joe Bayer (joexbayer)
 * @brief Tar file system.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <lib/tar.h>
#include <terminal.h>
#include <fs/fs.h>
#include <ksyms.h>
#include <memory.h>
#include <errors.h>
#include <libc.h>

/* The longest name a header holds: its 155-byte prefix, a "/", and its 100-byte name */
#define TAR_NAME_MAX (155 + 1 + 100)

/**
 * Parses an octal field: digits after any leading spaces, up to the first character that isn't one, which is
 * usually a space or a zero byte.
 * @return 0, or -1 if the field has no digits
 */
static int __tar_octal(const char* field, int size, unsigned int* out)
{
    unsigned int value = 0;
    int i = 0;
    int digits = 0;

    while(i < size && field[i] == ' ') i++;
    for(; i < size && field[i] >= '0' && field[i] <= '7'; i++, digits++){
        value = value * 8 + (field[i] - '0');
    }

    *out = value;
    return digits > 0 ? 0 : -1;
}

/* Copies a field that's terminated by a zero byte only when it's shorter than the field */
static int __tar_field(char* out, const char* field, int size)
{
    int len = 0;
    while(len < size && field[len] != 0) len++;
    memcpy(out, field, len);
    out[len] = 0;
    return len;
}

/**
 * Reads the next header from fd and checks it: that it's a ustar header, and that its data fits in what's left of
 * the file. Anything else is reported, rather than trusted, since tar can be given any file.
 * @return 1 with header, the entry's name (prefix, then name) in name, which holds TAR_NAME_MAX+1 bytes, and its
 * size in *size; 0 at the end of the archive; -1 if the file isn't a tar archive or is cut short
 */
static int __tar_next(int fd, struct tar_header* header, char* name, unsigned int* size)
{
    int bytes_read = fs_read(fd, header, sizeof(*header));
    if(bytes_read == 0) return 0;
    if(bytes_read < 0){
        twritef("tar: %s\n", error_get_string(bytes_read));
        return -1;
    }
    if(bytes_read < (int)sizeof(*header)){
        twritef("tar: Incomplete header.\n");
        return -1;
    }

    /* An archive ends with empty blocks */
    if(header->name[0] == '\0') return 0;

    /* "ustar" followed by a zero byte, or by spaces in GNU tar's own format */
    if(memcmp(header->magic, "ustar", 5) != 0){
        twritef("tar: Not a tar archive.\n");
        return -1;
    }

    int len = __tar_field(name, header->prefix, sizeof(header->prefix));
    if(len > 0) name[len++] = '/';
    len += __tar_field(name + len, header->name, sizeof(header->name));
    /* A directory's name ends in "/", which tar's output adds back */
    while(len > 1 && name[len-1] == '/') name[--len] = 0;

    int position = fs_seek(fd, 0, FS_SEEK_CUR);
    if(__tar_octal(header->size, sizeof(header->size), size) < 0 || position < 0 || *size > (unsigned int)(fs_size(fd) - position)){
        twritef("tar: %s: Bad size, or the archive is cut short.\n", name);
        return -1;
    }
    return 1;
}

/* Moves past an entry's data, which is padded to whole blocks */
static void __tar_skip(int fd, unsigned int size)
{
    unsigned int blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
    fs_seek(fd, blocks * TAR_BLOCK_SIZE, FS_SEEK_CUR);
}

static int tar_list(int fd)
{
    struct tar_header header;
    char name[TAR_NAME_MAX + 1];
    unsigned int size;
    int ret;

    while ((ret = __tar_next(fd, &header, name, &size)) == 1) {
        if(header.typeflag == TAR_TYPE_DIRECTORY){
            twritef("%s/\n", name);
        } else {
            twritef("%s (%d bytes)\n", name, size);
        }
        __tar_skip(fd, size);
    }

    return ret;
}

/* Writes size bytes of the archive, from its current position, into a file at path */
static int __tar_extract_file(int fd, const char* path, unsigned int size)
{
    /* kalloc_try, so an entry bigger than the free memory is an error rather than a panic */
    byte_t* buf = (byte_t*)kalloc_try(size > 0 ? size : 1);
    if(buf == NULL){
        twritef("tar: %s: %s\n", path, error_get_string(-ERROR_ALLOC));
        return -1;
    }

    int ret = fs_read(fd, buf, size);
    if(ret != (int)size){
        twritef("tar: %s: %s\n", path, ret < 0 ? error_get_string(ret) : "Cut short.");
        kfree(buf);
        return -1;
    }

    int new_file = fs_open(path, FS_FILE_FLAG_CREATE | FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE);
    if(new_file < 0){
        twritef("tar: %s: %s\n", path, error_get_string(new_file));
        kfree(buf);
        return -1;
    }

    ret = size > 0 ? fs_write(new_file, buf, size) : 0;
    fs_close(new_file);
    kfree(buf);
    if(ret != (int)size){
        twritef("tar: %s: %s\n", path, ret < 0 ? error_get_string(ret) : "Couldn't write it all.");
        return -1;
    }

    /* The data's padding to a whole block */
    fs_seek(fd, (TAR_BLOCK_SIZE - size % TAR_BLOCK_SIZE) % TAR_BLOCK_SIZE, FS_SEEK_CUR);
    return 0;
}

/* Extracts files and directories, relative to the current directory. Links and other kinds of entries are skipped. */
static int tar_extract(int fd)
{
    struct tar_header header;
    char name[TAR_NAME_MAX + 1];
    unsigned int size;
    int ret;

    while ((ret = __tar_next(fd, &header, name, &size)) == 1) {
        switch (header.typeflag){
        case TAR_TYPE_DIRECTORY:{
                twritef("%s/\n", name);
                int err = fs_mkdir(name);
                if(err < 0 && err != -ERROR_FILE_EXISTS){
                    twritef("tar: %s: %s\n", name, error_get_string(err));
                    return -1;
                }
                __tar_skip(fd, size);
            }
            break;
        case TAR_TYPE_FILE:
        case TAR_TYPE_FILE_OLD:
            twritef("%s\n", name);
            if(__tar_extract_file(fd, name, size) < 0) return -1;
            break;
        default:
            twritef("tar: %s: Skipped, not a file or directory.\n", name);
            __tar_skip(fd, size);
            break;
        }
    }

    return ret;
}

/* these functions are for the kernel */
int tar(int argc, char* argv[])
{
    if (argc != 3) {
        twritef("Usage: %s [options] <tarfile>\n", argv[0]);
        twritef("Options:\n");
        twritef("  -c: create a tar file\n");
        twritef("  -x: extract a tar file\n");
        twritef("  -t: list the contents of a tar file\n");
        return 1;
    }

    char* opts = argv[1];
    if(opts[0] != '-'){
        twritef("Invalid option: %s \n", argv[1]);
        return 1;
    }

    int fd = fs_open(argv[2], FS_FILE_FLAG_READ);
    if(fd < 0){
        twritef("tar: %s: %s\n", argv[2], error_get_string(fd));
        return 1;
    }

    switch (opts[1]) {
        case 'c':{
                twritef("ERROR: Currently unsupported!\n");
            }
            break;
        case 'x':{
                tar_extract(fd);
            }
            break;
        case 't':{
                tar_list(fd);
            }
            break;
        default:
            twritef("Invalid option: %s \n", argv[1]);
            return 1;
    }

    fs_close(fd);

    return 0;
}
EXPORT_KSYMBOL(tar);


