/*
 * gfx.h - tiny double-buffered 2D renderer for the PSP screen (480x272).
 *
 * Draws rectangles and pixel-font text at any scale straight into VRAM.
 * The font is the PSPSDK's built-in debug font, captured once at start-up.
 */
#ifndef POMO_GFX_H
#define POMO_GFX_H

#include <stdint.h>

#define SCREEN_W 480
#define SCREEN_H 272

/* Colors are 0xAABBGGRR on the PSP */
#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

enum { ICON_CROSS = 0, ICON_CIRCLE, ICON_SQUARE, ICON_TRIANGLE };

void gfx_init(void);
void gfx_clear(uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_frame(int x, int y, int w, int h, uint32_t color);   /* 1px outline */
int  gfx_text(int x, int y, int scale, uint32_t color, const char *s); /* returns end x */
int  gfx_text_width(const char *s, int scale);
void gfx_text_center(int y, int scale, uint32_t color, const char *s);
void gfx_text_right(int right_x, int y, int scale, uint32_t color, const char *s);
int  gfx_icon(int x, int y, int scale, int icon);              /* returns end x */
void gfx_present(void);   /* show the finished frame at the next vblank */
void gfx_wait_vblank(void);

#endif
