/**
 * @file icons.c
 * @brief Images the kernel loads from the disk (#177): ICONS_DIR NAME.bmp, read once and kept.
 */

#include <gfx/icons.h>
#include <bmp.h>
#include <fs/fs.h>
#include <memory.h>
#include <sync.h>
#include <libc.h>
#include <serial.h>
#include <errors.h>
#include <images.h>

/* How many images are kept, and the longest name */
#define ICONS_MAX 64
#define ICON_NAME_MAX 32

/* The largest file the kernel reads as an image: the longest header, a full color table and BMP_MAX_SIDE square rows */
#define ICON_FILE_MAX (14 + 124 + 256 * 4 + BMP_MAX_SIDE * BMP_MAX_SIDE)

static struct {
    char name[ICON_NAME_MAX];
    const struct image* image;
} icons[ICONS_MAX];
static int icon_count = 0;
static spinlock_t icons_lock = 0;

/* Reads ICONS_DIR name.bmp into a new image. @return it, or NULL, with why in the log */
static const struct image* icon_read(const char* name)
{
    char path[FS_PATH_MAX];
    int dir = strlen(ICONS_DIR);
    int len = strlen(name);
    memcpy(path, ICONS_DIR, dir);
    memcpy(path + dir, name, len);
    memcpy(path + dir + len, ".bmp", 5);

    struct fs_dirent info;
    int ret = fs_stat(path, &info);
    if(ret < 0){
        warningf("%s: %s\n", path, error_get_string(ret));
        return NULL;
    }
    if(info.size == 0 || info.size > ICON_FILE_MAX){
        warningf("%s: %d bytes is too big or too small for an image\n", path, info.size);
        return NULL;
    }

    unsigned char* data = kalloc_try(info.size);
    if(data == NULL){
        warningf("%s: %s\n", path, error_get_string(-ERROR_ALLOC));
        return NULL;
    }

    struct bmp_info bmp;
    int read = fs_load_from_file(path, data, info.size);
    if(read != (int)info.size || bmp_parse(data, read, &bmp) < 0){
        warningf("%s: %s\n", path, error_get_string(read < 0 ? read : -ERROR_BAD_IMAGE));
        kfree(data);
        return NULL;
    }

    struct image* image = kalloc_try(sizeof(struct image));
    unsigned char* pixels = kalloc_try(bmp.width * bmp.height);
    if(image == NULL || pixels == NULL){
        warningf("%s: %s\n", path, error_get_string(-ERROR_ALLOC));
        if(image != NULL) kfree(image);
        if(pixels != NULL) kfree(pixels);
        kfree(data);
        return NULL;
    }

    bmp_decode(data, &bmp, pixels);
    kfree(data);

    image->width = bmp.width;
    image->height = bmp.height;
    image->pixels = pixels;
    return image;
}

const struct image* icon_get(const char* name)
{
    if(name == NULL || name[0] == 0 || strlen(name) >= ICON_NAME_MAX){
        return &missing_image;
    }

    spin_lock(&icons_lock);
    for (int i = 0; i < icon_count; i++){
        if(strcmp(icons[i].name, name) == 0){
            const struct image* image = icons[i].image;
            spin_unlock(&icons_lock);
            return image;
        }
    }

    /* A file that can't be read is kept as the placeholder too, so it's tried, and warned about, once */
    const struct image* image = icon_read(name);
    if(image == NULL){
        image = &missing_image;
    }
    if(icon_count < ICONS_MAX){
        memcpy(icons[icon_count].name, name, strlen(name) + 1);
        icons[icon_count].image = image;
        icon_count++;
    }
    spin_unlock(&icons_lock);
    return image;
}
