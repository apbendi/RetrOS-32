#ifndef GFX_ICONS_H
#define GFX_ICONS_H

#include <image.h>

/**
 * The image ICONS_DIR NAME.bmp, read from the disk the first time it's asked for, and kept. If it's missing or can't be
 * read, the log says why, and it's missing_image, the placeholder, from then on. Images the kernel needs without a
 * disk are built in instead (images.h).
 */
const struct image* icon_get(const char* name);

#endif /* GFX_ICONS_H */
