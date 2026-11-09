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

static uint32_t *g_fb[2];          /* the two frame buffers in VRAM */
static uint32_t *g_draw;           /* where drawing goes: a buffer in normal RAM */
static int g_draw_idx;             /* VRAM buffer that receives the next frame */
/* Drawing into cached RAM is much faster than into VRAM (especially blending,
   which reads pixels back). The finished frame is copied to VRAM at present. */
static uint32_t g_ram[BUF_W * SCREEN_H] __attribute__((aligned(64)));
static unsigned char g_font[256][8];   /* 8x8 glyph bitmaps, one byte per row */
static int g_cx0 = 0, g_cy0 = 0, g_cx1 = SCREEN_W, g_cy1 = SCREEN_H;   /* clip rect */

void gfx_set_clip(int x, int y, int w, int h)
{
    g_cx0 = x < 0 ? 0 : x;
    g_cy0 = y < 0 ? 0 : y;
    g_cx1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    g_cy1 = y + h > SCREEN_H ? SCREEN_H : y + h;
}

void gfx_reset_clip(void)
{
    g_cx0 = 0; g_cy0 = 0; g_cx1 = SCREEN_W; g_cy1 = SCREEN_H;
}

static int clip_rect(int *x, int *y, int *w, int *h)
{
    if (*x < g_cx0) { *w -= g_cx0 - *x; *x = g_cx0; }
    if (*y < g_cy0) { *h -= g_cy0 - *y; *y = g_cy0; }
    if (*x + *w > g_cx1) *w = g_cx1 - *x;
    if (*y + *h > g_cy1) *h = g_cy1 - *y;
    return *w > 0 && *h > 0;
}

static inline uint32_t blend_px(uint32_t p, uint32_t color, int alpha)
{
    uint32_t r = ((p & 0xFF) * (255 - alpha) + (color & 0xFF) * alpha) / 255;
    uint32_t g = (((p >> 8) & 0xFF) * (255 - alpha) + ((color >> 8) & 0xFF) * alpha) / 255;
    uint32_t b = (((p >> 16) & 0xFF) * (255 - alpha) + ((color >> 16) & 0xFF) * alpha) / 255;
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

static void sprite_blit(const Sprite *s, int sy0, int sh, int x, int y, int flags,
                        uint32_t glass_color, int glass_alpha)
{
    int dx0 = x, dy0 = y, w = s->w, h = sh, i, j;
    int ox, oy;
    if (!clip_rect(&dx0, &dy0, &w, &h)) return;
    ox = dx0 - x;   /* first visible column / row of the sprite */
    oy = dy0 - y;
    for (j = 0; j < h; j++) {
        int sy = sy0 + oy + j;
        const uint8_t *src = s->px + sy * s->w;
        const uint8_t *gl = s->glass ? s->glass + sy * s->w : 0;
        uint32_t *dst = g_draw + (dy0 + j) * BUF_W + dx0;
        for (i = 0; i < w; i++) {
            int sx = (flags & SPR_FLIP_X) ? (s->w - 1 - (ox + i)) : (ox + i);
            uint8_t k = src[sx];
            uint32_t c;
            if (!k) continue;
            c = s->pal[k];
            if (gl && gl[sx] && glass_alpha > 0) c = blend_px(c, glass_color, glass_alpha);
            dst[i] = c;
        }
    }
}

void gfx_sprite(const Sprite *s, int x, int y, int flags)
{
    sprite_blit(s, 0, s->h, x, y, flags, 0, 0);
}

void gfx_sprite_lit(const Sprite *s, int x, int y, int flags, uint32_t glass_color, int glass_alpha)
{
    sprite_blit(s, 0, s->h, x, y, flags, glass_color, glass_alpha);
}

void gfx_sprite_rows(const Sprite *s, int sy, int sh, int x, int y)
{
    if (sy < 0) { sh += sy; y -= sy; sy = 0; }
    if (sy + sh > s->h) sh = s->h - sy;
    if (sh > 0) sprite_blit(s, sy, sh, x, y, 0, 0, 0);
}

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
    uint32_t *g_draw = g_fb[1];    /* the debug font draws into VRAM */
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
    pspDebugScreenSetBase((u32 *)g_fb[1]);
    capture_font();

    g_draw = g_ram;
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
    if (!clip_rect(&x, &y, &w, &h)) return;

    for (j = 0; j < h; j++) {
        uint32_t *row = g_draw + (y + j) * BUF_W + x;
        for (i = 0; i < w; i++)
            row[i] = color;
    }
}

void gfx_blend_rect(int x, int y, int w, int h, uint32_t color, int alpha)
{
    int i, j;
    uint32_t cr = color & 0xFF, cg = (color >> 8) & 0xFF, cb = (color >> 16) & 0xFF;
    if (alpha <= 0) return;
    if (alpha >= 255) { gfx_rect(x, y, w, h, color); return; }
    if (!clip_rect(&x, &y, &w, &h)) return;

    for (j = 0; j < h; j++) {
        uint32_t *row = g_draw + (y + j) * BUF_W + x;
        for (i = 0; i < w; i++) {
            uint32_t p = row[i];
            uint32_t r = ((p & 0xFF) * (255 - alpha) + cr * alpha) / 255;
            uint32_t g = (((p >> 8) & 0xFF) * (255 - alpha) + cg * alpha) / 255;
            uint32_t b = (((p >> 16) & 0xFF) * (255 - alpha) + cb * alpha) / 255;
            row[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
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
    uint32_t *dst = g_fb[g_draw_idx];
    int y;
    for (y = 0; y < SCREEN_H; y++)
        memcpy(dst + y * BUF_W, g_ram + y * BUF_W, SCREEN_W * sizeof(uint32_t));

    sceDisplaySetFrameBuf(dst, BUF_W, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
    g_draw_idx ^= 1;
}

void gfx_wait_vblank(void)
{
    sceDisplayWaitVblankStart();
}
