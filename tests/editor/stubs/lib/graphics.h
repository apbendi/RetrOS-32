/**
 * Host stand-in for include/lib/graphics.h, whose extern "C" block would give the stub exit() C linkage, clashing with
 * the host's. The declarations and COLOR_BAR_* are copied from it; keep them in step.
 */
#ifndef EDITOR_TEST_LIB_GRAPHICS_H
#define EDITOR_TEST_LIB_GRAPHICS_H
#include <lib/syscall.h>
#include <colors.h>
#include <args.h>
#include <gfx/events.h>
#include <image.h>

#define COLOR_BAR_FACE 30
#define COLOR_BAR_LIGHT COLOR_VGA_LIGHTEST_GRAY
#define COLOR_BAR_SHADOW (COLOR_VGA_MEDIUM_DARK_GRAY + 5)

int gfx_draw_text(int x, int y, const char* text, unsigned char color);
int gfx_draw_format_text(int x, int y, unsigned char color, const char* fmt, ...);
int gfx_draw_char(int x, int y, char data, unsigned char color);
int gfx_draw_rectangle(int x, int y, int width, int height, unsigned char color);
int gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color);
int gfx_get_event(struct gfx_event*, gfx_event_flag_t flags);
int gfx_end_frame(void);
int gfx_draw_image(int x, int y, const struct image* image);
int gfx_load_icon(const char* name, struct image* image);
void gfx_free_icon(struct image* image);
void gfx_draw_bevel(int x, int y, int width, int height, int pressed);
#endif
