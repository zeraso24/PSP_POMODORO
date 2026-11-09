/*
 * gfx.c - double-buffered drawing directly into the PSP's video memory.
 *
 * Two 512x272 32-bit frame buffers live in VRAM. We always draw into the one
 * that is not on screen, then flip at vertical blank, so there is no flicker.
 */
#include "gfx.h"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspdebug.h>
#include <string.h>

#define BUF_W 512   /* frame buffer stride in pixels */

static uint32_t *g_fb[2];
static uint32_t *g_draw;
static int g_draw_idx;
static unsigned char g_font[256][8];   /* 8x8 glyph bitmaps, one byte per row */

static const unsigned char ICONS[4][8] = {
    /* cross */    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 },
    /* circle */   { 0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C },
    /* square */   { 0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF },
    /* triangle */ { 0x18, 0x18, 0x24, 0x24, 0x42, 0x42, 0xFF, 0x00 },
};
static const uint32_t ICON_COLORS[4] = {
    RGB(124, 166, 232),  /* cross: blue */
    RGB(232, 106, 106),  /* circle: red */
    RGB(216, 140, 200),  /* square: pink */
    RGB(79, 209, 165),   /* triangle: green */
};

static void capture_font(void)
{
    int ch, x, y;
    /* Render each glyph with the SDK's debug font into the hidden buffer at
       (0,0), read the pixels back and keep them as a bitmap. The hidden
       buffer is cleared before it is ever shown. */
    for (ch = 0; ch < 256; ch++) {
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                g_draw[y * BUF_W + x] = 0;

        pspDebugScreenPutChar(0, 0, 0xFFFFFFFFu, (unsigned char)ch);

        for (y = 0; y < 8; y++) {
            unsigned char row = 0;
            for (x = 0; x < 8; x++)
                if (g_draw[y * BUF_W + x] == 0xFFFFFFFFu)
                    row |= (unsigned char)(0x80 >> x);
            g_font[ch][y] = row;
        }
    }
}

void gfx_init(void)
{
    /* 0x40000000 | address = uncached access to VRAM */
    uint32_t *vram = (uint32_t *)(0x40000000u | (uintptr_t)sceGeEdramGetAddr());
    g_fb[0] = vram;
    g_fb[1] = vram + BUF_W * SCREEN_H;

    /* Sets 32-bit display mode and shows buffer 0 */
    pspDebugScreenInitEx(g_fb[0], PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
    pspDebugScreenEnableBackColor(0);

    g_draw_idx = 1;
    g_draw = g_fb[1];
    pspDebugScreenSetBase((u32 *)g_draw);

    capture_font();
    gfx_clear(0xFF000000u);
}

void gfx_clear(uint32_t color)
{
    int x, y;
    for (y = 0; y < SCREEN_H; y++) {
        uint32_t *row = g_draw + y * BUF_W;
        for (x = 0; x < SCREEN_W; x++)
            row[x] = color;
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t color)
{
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    for (j = 0; j < h; j++) {
        uint32_t *row = g_draw + (y + j) * BUF_W + x;
        for (i = 0; i < w; i++)
            row[i] = color;
    }
}

void gfx_frame(int x, int y, int w, int h, uint32_t color)
{
    gfx_rect(x, y, w, 1, color);
    gfx_rect(x, y + h - 1, w, 1, color);
    gfx_rect(x, y, 1, h, color);
    gfx_rect(x + w - 1, y, 1, h, color);
}

static void draw_bitmap(int x, int y, int scale, uint32_t color, const unsigned char rows[8])
{
    int gx, gy;
    for (gy = 0; gy < 8; gy++) {
        unsigned char row = rows[gy];
        if (!row) continue;
        for (gx = 0; gx < 8; gx++)
            if (row & (0x80 >> gx))
                gfx_rect(x + gx * scale, y + gy * scale, scale, scale, color);
    }
}

int gfx_text(int x, int y, int scale, uint32_t color, const char *s)
{
    if (scale < 1) scale = 1;
    while (*s) {
        draw_bitmap(x, y, scale, color, g_font[(unsigned char)*s]);
        x += 8 * scale;
        s++;
    }
    return x;
}

int gfx_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 8 * (scale < 1 ? 1 : scale);
}

void gfx_text_center(int y, int scale, uint32_t color, const char *s)
{
    gfx_text((SCREEN_W - gfx_text_width(s, scale)) / 2, y, scale, color, s);
}

void gfx_text_right(int right_x, int y, int scale, uint32_t color, const char *s)
{
    gfx_text(right_x - gfx_text_width(s, scale), y, scale, color, s);
}

int gfx_icon(int x, int y, int scale, int icon)
{
    if (icon < 0 || icon > 3) return x;
    draw_bitmap(x, y, scale, ICON_COLORS[icon], ICONS[icon]);
    return x + 8 * scale;
}

void gfx_present(void)
{
    sceDisplaySetFrameBuf(g_draw, BUF_W, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();

    g_draw_idx ^= 1;
    g_draw = g_fb[g_draw_idx];
}

void gfx_wait_vblank(void)
{
    sceDisplayWaitVblankStart();
}
