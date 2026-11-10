/*
 * city.c - animated pixel-art city: towers, cranes, street life, fireworks.
 * Original pixel art, styled after side-scrolling city packs.
 */
#include "city.h"
#include "gfx.h"
#include "assets.h"

#define SEASON_TIERS_CITY 5
#include <math.h>

#define C_FACADE    RGB(56, 62, 86)
#define C_FACADE_HI RGB(78, 86, 114)
#define C_FACADE_SH RGB(44, 48, 70)
#define C_CORNICE   RGB(98, 106, 138)
#define C_WIN_OFF   RGB(28, 30, 44)
#define C_CONCRETE  RGB(104, 104, 118)
#define C_SCAFF     RGB(214, 170, 70)
#define C_CRANE     RGB(242, 192, 64)
#define C_ROOF      RGB(234, 88, 66)
#define C_ROOF_DK   RGB(170, 60, 46)
#define C_FLAG      RGB(240, 240, 246)
#define C_DOOR      RGB(24, 24, 34)
#define C_METAL     RGB(150, 152, 166)

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = (a * 2654435761u) ^ (b * 2246822519u) ^ (c * 3266489917u) ^ 0x9E3779B9u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return h;
}

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return RGB(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t), ab + (int)((bb - ab) * t));
}

uint32_t city_window_color(int hour)
{
    if (hour < 0 || hour > 23) return RGB(255, 212, 110);   /* unknown hour */
    if (hour < 12) return RGB(255, 212, 110);   /* morning: warm yellow */
    if (hour < 18) return RGB(255, 150, 78);    /* afternoon: orange */
    return RGB(128, 182, 255);                  /* evening: blue */
}

/* ------------------------------------------------------------------ */
/* primitives                                                           */
/* ------------------------------------------------------------------ */

void city_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2;
        gfx_rect(x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void city_disc(int cx, int cy, int r, uint32_t color)
{
    int y;
    for (y = -r; y <= r; y++) {
        int half = (int)sqrtf((float)(r * r - y * y) + 0.3f);
        gfx_rect(cx - half, cy + y, 2 * half + 1, 1, color);
    }
}

void city_glow(int cx, int cy, int r, uint32_t color, int alpha)
{
    int x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            float d = sqrtf((float)(x * x + y * y)) / (float)r;
            if (d < 1.0f)
                gfx_blend_rect(cx + x, cy + y, 1, 1, color, (int)(alpha * (1.0f - d) * (1.0f - d)));
        }
}

/* ------------------------------------------------------------------ */
/* sky                                                                  */
/* ------------------------------------------------------------------ */

void city_sky_rect(int x0, int y0, int w, int h)
{
    int y, i;
    for (y = 0; y < h; y++) {
        float t = (float)y / (float)h;
        int r = (int)(14 + t * 30), g = (int)(16 + t * 14), b = (int)(30 + t * 28);
        gfx_rect(x0, y0 + y, w, 1, RGB(r, g, b));
    }
    for (i = 0; i < (w * h) / 1300 + 1; i++) {
        uint32_t hs = hash3((uint32_t)i, 77, (uint32_t)x0);
        int sx = x0 + (int)(hs % (unsigned)w);
        int sy = y0 + (int)((hs >> 9) % (unsigned)(h * 2 / 3 + 1));
        int lum = 70 + (int)((hs >> 20) % 90);
        gfx_rect(sx, sy, 1, 1, RGB(lum, lum, lum + 20));
    }
}

void city_sky(int top, int bottom)
{
    city_sky_rect(0, top, SCREEN_W, bottom - top);
}

/* Sky colors follow the real clock: night, dawn, day, dusk. */
void city_sky_time(int top, int bottom, int minute_of_day, int anim)
{
    /* top color, horizon color, star amount for 5 key times */
    static const int key_min[6] = { 0, 360, 480, 1020, 1200, 1440 };
    static const uint32_t key_top[6] = {
        RGB(10, 12, 28), RGB(44, 50, 100), RGB(64, 120, 196),
        RGB(64, 120, 196), RGB(20, 18, 48), RGB(10, 12, 28) };
    static const uint32_t key_hor[6] = {
        RGB(30, 26, 54), RGB(232, 150, 120), RGB(170, 210, 240),
        RGB(170, 210, 240), RGB(176, 92, 104), RGB(30, 26, 54) };
    static const int key_stars[6] = { 100, 20, 0, 0, 60, 100 };
    int k = 0, y, i, stars;
    float t;
    uint32_t ctop, chor;

    while (k < 4 && minute_of_day >= key_min[k + 1]) k++;
    t = (float)(minute_of_day - key_min[k]) / (float)(key_min[k + 1] - key_min[k]);
    ctop = mix(key_top[k], key_top[k + 1], t);
    chor = mix(key_hor[k], key_hor[k + 1], t);
    stars = key_stars[k] + (int)((key_stars[k + 1] - key_stars[k]) * t);

    for (y = top; y < bottom; y++) {
        float f = (float)(y - top) / (float)(bottom - top);
        gfx_rect(0, y, SCREEN_W, 1, mix(ctop, chor, f * f));
    }
    for (i = 0; i < stars; i++) {   /* twinkling stars */
        uint32_t hs = hash3((uint32_t)i, 91, 3);
        int sx = (int)(hs % SCREEN_W);
        int sy = top + (int)((hs >> 9) % (unsigned)((bottom - top) / 2 + 1));
        float tw = 0.5f + 0.5f * sinf(anim * 0.11f + (float)(hs % 628) * 0.01f);
        int lum = 60 + (int)(tw * (float)(40 + (hs >> 24) % 120));
        gfx_rect(sx, sy, 1, 1, RGB(lum, lum, lum + 25 > 255 ? 255 : lum + 25));
    }
}

void city_clouds(int top, int bottom, int anim)
{
    int i;
    for (i = 0; i < 4; i++) {
        uint32_t hs = hash3((uint32_t)i, 5, 11);
        int w = 34 + (int)(hs % 40);
        int span = SCREEN_W + w * 2;
        int x = (int)(((hs >> 8) % (unsigned)span) + (unsigned)(anim / (3 + i))) % span - w;
        int y = top + (int)((hs >> 16) % (unsigned)(bottom - top));
        gfx_blend_rect(x, y, w, 3, RGB(200, 170, 200), 34);
        gfx_blend_rect(x + w / 5, y - 2, w * 3 / 5, 2, RGB(200, 170, 200), 30);
        gfx_blend_rect(x + w / 3, y - 4, w / 3, 2, RGB(200, 170, 200), 26);
    }
}

void city_skyline(int ground_y, int min_h, int max_h, uint32_t body, uint32_t window, uint32_t seed)
{
    int x = -6, i = 0;
    while (x < SCREEN_W) {
        uint32_t hs = hash3(seed, (uint32_t)i++, 1);
        int w = 16 + (int)(hs % 26);
        int h = min_h + (int)((hs >> 8) % (unsigned)(max_h - min_h + 1));
        int top = ground_y - h, wy, wx;
        gfx_rect(x, top, w, h, body);
        if ((hs >> 20) % 3 == 0)                    /* stepped top */
            gfx_rect(x + w / 4, top - 5, w / 2, 5, body);
        if ((hs >> 22) % 4 == 0)                    /* antenna */
            gfx_rect(x + w / 2, top - 12, 1, 12, body);
        for (wy = top + 4; wy < ground_y - 4; wy += 5)   /* sparse windows */
            for (wx = x + 3; wx < x + w - 3; wx += 4)
                if (hash3(seed, (uint32_t)(wx * 7 + wy), 9) % 9 == 0)
                    gfx_rect(wx, wy, 2, 1, window);
        x += w + (int)((hs >> 26) % 4);
    }
}

void city_ground(int ground_y, int bottom)
{
    gfx_rect(0, ground_y, SCREEN_W, 2, RGB(78, 78, 96));
    gfx_rect(0, ground_y + 2, SCREEN_W, bottom - ground_y - 2, RGB(24, 24, 32));
}

void city_street(int sidewalk_y, int road_bottom, int anim)
{
    int x, road_y = sidewalk_y + 8;
    (void)anim;
    /* sidewalk with paving joints */
    gfx_rect(0, sidewalk_y, SCREEN_W, 8, RGB(70, 68, 84));
    gfx_rect(0, sidewalk_y, SCREEN_W, 1, RGB(96, 94, 112));
    for (x = 0; x < SCREEN_W; x += 12)
        gfx_rect(x, sidewalk_y + 1, 1, 7, RGB(60, 58, 74));
    gfx_rect(0, road_y - 1, SCREEN_W, 1, RGB(110, 108, 124));    /* curb */
    /* asphalt with a dashed center line */
    gfx_rect(0, road_y, SCREEN_W, road_bottom - road_y, RGB(30, 30, 38));
    for (x = 0; x < SCREEN_W; x += 20)
        gfx_rect(x, road_y + (road_bottom - road_y) / 2, 10, 1, RGB(150, 140, 100));
}

/* ------------------------------------------------------------------ */
/* towers                                                               */
/* ------------------------------------------------------------------ */

static void draw_crane(int mast_x, int base_y, int height, int reach_l, int reach_r, int anim)
{
    int top = base_y - height, y, tx, cl;
    float s1 = 0.5f + 0.5f * sinf(anim * 0.05f);
    float s2 = 0.5f + 0.5f * sinf(anim * 0.08f + 1.3f);

    gfx_rect(mast_x, top, 1, height, C_CRANE);              /* lattice mast */
    gfx_rect(mast_x + 3, top, 1, height, C_CRANE);
    for (y = top + 2; y < base_y; y += 4)
        city_line(mast_x, y, mast_x + 3, y + 2, C_CRANE);
    gfx_rect(mast_x - reach_l, top, reach_l + reach_r + 4, 2, C_CRANE);   /* jib */
    gfx_rect(mast_x + reach_r, top - 1, 4, 4, C_METAL);     /* counterweight */
    city_line(mast_x + 1, top - 5, mast_x - reach_l, top, C_CRANE);
    city_line(mast_x + 2, top - 5, mast_x + reach_r + 2, top, C_CRANE);
    gfx_rect(mast_x + 1, top - 5, 2, 5, C_CRANE);
    gfx_rect(mast_x - 1, top + 2, 6, 4, C_CRANE);           /* cab */
    gfx_rect(mast_x, top + 3, 2, 2, RGB(255, 220, 140));
    if ((anim / 14) % 2 == 0)                               /* warning light */
        gfx_rect(mast_x - reach_l, top - 1, 1, 1, RGB(255, 70, 60));

    /* trolley slides along the jib, the hook lifts a beam */
    tx = mast_x - reach_l + 3 + (int)((float)(reach_l - 9) * s1);
    cl = 4 + (int)((float)height * 0.55f * s2);
    gfx_rect(tx - 1, top + 2, 3, 2, C_METAL);
    gfx_rect(tx, top + 4, 1, cl, RGB(200, 200, 210));
    gfx_rect(tx - 4, top + 4 + cl, 9, 2, C_CONCRETE);
}

/* small-tower palettes: default + the three pack styles */
typedef struct { uint32_t facade, hi, sh, cornice; } Palette;
static const Palette PALS[4] = {
    { C_FACADE,           C_FACADE_HI,        C_FACADE_SH,        C_CORNICE },
    { RGB(138, 111, 109), RGB(205, 195, 197), RGB(110, 88, 88),   RGB(191, 167, 169) },  /* brownstone */
    { RGB(187, 136, 113), RGB(228, 208, 213), RGB(150, 104, 88),  RGB(191, 167, 169) },  /* shop */
    { RGB(138, 83, 77),   RGB(216, 187, 184), RGB(104, 62, 58),   RGB(191, 167, 169) },  /* club */
};

void city_tower(const Tower *t)
{
    const Palette *pal0 = &PALS[(t->style >= 0 && t->style < 4) ? t->style : 0];
    Palette skinned;
    const Palette *pal = pal0;
    if (t->skin > 0 && t->style > 0) {
        float hue = t->style == 2 ? 19.0f : 5.0f;
        skinned.facade  = city_skin_color(pal0->facade, t->skin, hue);
        skinned.hi      = city_skin_color(pal0->hi, t->skin, hue);
        skinned.sh      = city_skin_color(pal0->sh, t->skin, hue);
        skinned.cornice = city_skin_color(pal0->cornice, t->skin, hue);
        pal = &skinned;
    }
    int n = t->windows < 1 ? 1 : t->windows;
    int drawn = t->floors < t->goal ? t->floors : t->goal;
    int gap = t->w >= 60 ? 6 : (t->w >= 30 ? 3 : 1);
    int ww = (t->w - 4 - (n - 1) * gap) / n;
    int used, m, wh, wy_off, i, j, shade_x;

    if (ww < 1) ww = 1;
    used = n * ww + (n - 1) * gap;
    m = (t->w - used) / 2;
    wh = t->floor_h / 2; if (wh < 2) wh = 2;
    wy_off = (t->floor_h - wh) / 2;
    shade_x = t->x + (t->w * 3) / 4;

    for (i = 0; i < drawn; i++) {
        int fy = t->ground_y - (i + 1) * t->floor_h;
        int hour = t->hours ? t->hours[i] : -1;
        uint32_t lit = city_window_color(hour);

        gfx_rect(t->x, fy, t->w, t->floor_h, pal->facade);
        gfx_rect(shade_x, fy, t->x + t->w - shade_x, t->floor_h, pal->sh);   /* side shadow */
        gfx_rect(t->x, fy, 1, t->floor_h, pal->hi);
        if (t->style) gfx_rect(t->x + t->w - 1, fy, 1, t->floor_h, pal->hi);

        for (j = 0; j < n; j++) {
            int wx = t->x + m + j * (ww + gap);
            uint32_t c = lit;
            int off = (hash3(t->seed, (uint32_t)i, (uint32_t)j) % 6 == 0);
            if (i == 0 && t->w >= 60 && j == n / 2) {          /* ground floor door */
                gfx_rect(wx, fy + 2, ww, t->floor_h - 2, C_DOOR);
                gfx_rect(wx + ww / 2, fy + 2, 1, t->floor_h - 2, RGB(60, 60, 76));
                continue;
            }
            if (t->anim && hash3(t->seed, (uint32_t)(i * 16 + j), (uint32_t)(t->anim / 45)) % 53 == 0)
                off = !off;                                    /* someone switches a light */
            if (t->reveal > 0 && i == drawn - 1)
                off = (j >= t->reveal - 1);
            if (off) c = C_WIN_OFF;
            if (i == t->highlight) c = RGB(255, 255, 228);
            gfx_rect(wx, fy + wy_off, ww, wh, c);
            if (!off && wx + ww > shade_x)
                gfx_blend_rect(wx, fy + wy_off, ww, wh, RGB(0, 0, 0), 40);
        }

        if ((i + 1) % 4 == 0 && (i + 1) < t->goal)        /* cornice per cycle */
            gfx_rect(t->x - 1, fy, t->w + 2, t->floor_h >= 8 ? 2 : 1, pal->cornice);

        if (i == t->highlight) {
            int sy = fy + t->floor_h / 2;
            uint32_t s = RGB(255, 255, 228);
            int k = (t->anim / 4) % 2;
            gfx_rect(t->x - 6 - k, sy, 3 + 2 * k, 1, s); gfx_rect(t->x - 5, sy - 1 - k, 1, 3 + 2 * k, s);
            gfx_rect(t->x + t->w + 3, sy - 3, 3 + 2 * k, 1, s); gfx_rect(t->x + t->w + 4 + k, sy - 4 - k, 1, 3 + 2 * k, s);
        }
    }

    /* rooftop clutter on unfinished days */
    if (t->floors > 0 && t->floors < t->goal && !t->building && t->w >= 30) {
        int top = t->ground_y - drawn * t->floor_h;
        gfx_rect(t->x + t->w - 13, top - 7, 7, 5, RGB(96, 80, 72));          /* water tank */
        gfx_rect(t->x + t->w - 13, top - 8, 7, 1, RGB(120, 100, 88));
        gfx_rect(t->x + t->w - 12, top - 2, 1, 2, C_METAL);
        gfx_rect(t->x + t->w - 8, top - 2, 1, 2, C_METAL);
        gfx_rect(t->x + 5, top - 3, 6, 3, RGB(110, 112, 126));               /* AC unit */
    }

    /* roof */
    if (t->floors >= t->goal) {
        int top = t->ground_y - t->goal * t->floor_h - t->roof_drop;
        int big = t->w >= 30;
        int pole_x = t->x + t->w / 2;
        int ph = big ? 14 : 7;
        gfx_rect(t->x - 2, top - 3, t->w + 4, 3, t->style ? pal->cornice : C_ROOF);
        gfx_rect(t->x - 2, top - 1, t->w + 4, 1, t->style ? pal->sh : C_ROOF_DK);
        if (t->roof_drop == 0 && t->floors > t->goal) {
            int ah = (big ? 6 : 3) + (t->floors - t->goal) * (big ? 5 : 3);
            gfx_rect(pole_x, top - 3 - ah, 1, ah, RGB(190, 190, 200));
            if (!t->anim || (t->anim / 12) % 2 == 0) {
                gfx_rect(pole_x - 1, top - 5 - ah, 3, 2, RGB(255, 60, 60));
                if (t->anim) city_glow(pole_x, top - 4 - ah, 5, RGB(255, 60, 60), 90);
            }
        } else if (t->roof_drop == 0) {
            int fw = big ? 8 : 4, fh = big ? 5 : 3, r;
            int pct = t->flag_pct > 0 ? t->flag_pct : 100;
            int fy = top - 3 - ph + (int)((float)(ph - fh) * (1.0f - (float)pct / 100.0f));
            gfx_rect(pole_x, top - 3 - ph, 1, ph, RGB(190, 190, 200));
            for (r = 0; r < fh; r++) {                        /* waving flag */
                int shift = t->anim ? (((t->anim / 5) + r) % 3 == 0) : 0;
                gfx_rect(pole_x + 1 + shift, fy + r, fw - shift, 1,
                         (big && r == fh / 2) ? C_ROOF : C_FLAG);
            }
        }
    }

    /* floor under construction */
    if (t->building && t->floors < t->goal) {
        int fy = t->ground_y - (t->floors + 1) * t->floor_h;
        int h = (int)(t->floor_h * t->progress);
        int k;
        gfx_rect(t->x, fy + t->floor_h - h, t->w, h, C_CONCRETE);
        gfx_frame(t->x - 2, fy, t->w + 4, t->floor_h + 1, C_SCAFF);
        for (k = 0; k + t->floor_h <= t->w + 4; k += t->floor_h)
            city_line(t->x - 2 + k, fy + t->floor_h, t->x - 2 + k + t->floor_h, fy, C_SCAFF);

        if (t->anim) {                                         /* welding sparks */
            int s = t->anim % 24;
            if (s < 9) {
                uint32_t hs = hash3(t->seed, (uint32_t)(t->anim / 24), 4);
                int ex = t->x + 3 + (int)(hs % (unsigned)(t->w - 6));
                int ey = fy + t->floor_h - h;
                int p;
                if (s < 3) city_glow(ex, ey, 6, RGB(255, 230, 160), 120);
                for (p = 0; p < 5; p++) {
                    int dx = (int)(hash3(hs, (uint32_t)p, 2) % 7) - 3;
                    gfx_rect(ex + dx * s / 2, ey - 2 + (s * s) / 6 - (s * (p % 2)), 1, 1,
                             s < 4 ? RGB(255, 250, 210) : RGB(255, 160, 60));
                }
            }
        }
        if (t->crane) {
            int ch = t->floor_h * 3 + 8;
            draw_crane(t->x + t->w - 8, fy, ch, t->w * 3 / 4, t->w / 4 + 2, t->anim);
        }
    }
}

/* a future day: hoarding wall with graffiti; a day off: bare foundation */
void city_lot(int x, int ground_y, int w, int future)
{
    int i;
    if (future) {
        static const uint32_t tag[5] = { RGB(232, 92, 170), RGB(80, 210, 230),
                                         RGB(240, 210, 80), RGB(120, 220, 120), RGB(250, 250, 250) };
        int h = w >= 30 ? 16 : 6;
        uint32_t hs = hash3((uint32_t)x, 31, 7);
        gfx_rect(x, ground_y - h, w, h, RGB(84, 70, 64));
        for (i = 3; i < w; i += 5)
            gfx_rect(x + i, ground_y - h, 1, h, RGB(68, 56, 52));
        gfx_rect(x, ground_y - h, w, 1, RGB(104, 88, 80));
        if (w >= 30) {
            for (i = 0; i < 3; i++) {
                uint32_t c = tag[(hs >> (i * 3)) % 5];
                int gx = x + 3 + (int)((hs >> (i * 5)) % (unsigned)(w - 14));
                int gy = ground_y - h + 4 + (int)((hs >> (i * 4)) % 6);
                city_line(gx, gy + 4, gx + 3, gy, c);
                city_line(gx + 3, gy, gx + 6, gy + 5, c);
                city_line(gx + 6, gy + 5, gx + 10, gy + 1, c);
            }
        }
    } else {
        gfx_rect(x, ground_y - 3, w, 3, RGB(58, 58, 72));
        gfx_rect(x, ground_y - 3, w, 1, RGB(72, 72, 88));
    }
}

/* ------------------------------------------------------------------ */
/* street life                                                          */
/* ------------------------------------------------------------------ */

void city_lamp(int x, int ground_y, int anim, uint32_t seed)
{
    int top = ground_y - 30;
    int on = 1;
    if (seed % 5 == 0 && anim && (hash3(seed, (uint32_t)(anim / 3), 1) % 9 == 0)) on = 0;  /* flicker */
    gfx_rect(x, top, 1, 30, RGB(70, 72, 86));
    gfx_rect(x - 1, ground_y - 2, 3, 2, RGB(70, 72, 86));
    gfx_rect(x, top, 6, 1, RGB(70, 72, 86));
    gfx_rect(x + 4, top + 1, 4, 2, on ? RGB(255, 226, 160) : RGB(90, 86, 80));
    if (on) {
        city_glow(x + 6, top + 3, 14, RGB(255, 210, 140), 70);
        gfx_blend_rect(x - 2, ground_y, 16, 2, RGB(255, 210, 140), 40);
    }
}

void city_tree(int x, int ground_y, int variant, int anim)
{
    static const uint32_t pal[3][3] = {
        { RGB(34, 96, 64),  RGB(50, 132, 80),  RGB(86, 172, 98) },    /* green */
        { RGB(150, 70, 36), RGB(204, 108, 44), RGB(240, 156, 70) },   /* orange */
        { RGB(86, 52, 120), RGB(124, 74, 168), RGB(170, 110, 214) },  /* purple */
    };
    const uint32_t *p = pal[variant % 3];
    int sway = anim ? (int)(sinf(anim * 0.07f + x) * 1.2f) : 0;
    int cx = x + 6, cy = ground_y - 20;

    gfx_rect(x, ground_y - 5, 13, 5, RGB(110, 80, 70));       /* planter */
    gfx_rect(x, ground_y - 5, 13, 1, RGB(140, 104, 90));
    gfx_rect(cx, ground_y - 13, 2, 8, RGB(86, 60, 44));       /* trunk */
    city_disc(cx + sway, cy, 7, p[0]);
    city_disc(cx - 4 + sway, cy + 2, 5, p[0]);
    city_disc(cx + 5 + sway, cy + 2, 5, p[0]);
    city_disc(cx - 1 + sway, cy - 1, 5, p[1]);
    city_disc(cx + 3 + sway, cy + 1, 3, p[1]);
    city_disc(cx - 2 + sway, cy - 3, 2, p[2]);
}

void city_bus_stop(int x, int ground_y)
{
    gfx_rect(x, ground_y - 20, 1, 20, RGB(90, 92, 108));
    city_disc(x, ground_y - 22, 3, RGB(60, 120, 210));
    gfx_rect(x - 1, ground_y - 23, 3, 1, RGB(240, 240, 246));
    gfx_rect(x + 3, ground_y - 12, 10, 8, RGB(40, 44, 60));   /* shelter panel */
    gfx_blend_rect(x + 4, ground_y - 11, 8, 6, RGB(140, 200, 240), 60);
}

static void wheel(int cx, int cy, int anim)
{
    int a = (anim / 2) % 4;
    city_disc(cx, cy, 4, RGB(20, 20, 26));
    city_disc(cx, cy, 2, RGB(120, 122, 136));
    if (a == 0) gfx_rect(cx - 2, cy, 5, 1, RGB(40, 40, 48));
    if (a == 1) city_line(cx - 2, cy - 2, cx + 2, cy + 2, RGB(40, 40, 48));
    if (a == 2) gfx_rect(cx, cy - 2, 1, 5, RGB(40, 40, 48));
    if (a == 3) city_line(cx - 2, cy + 2, cx + 2, cy - 2, RGB(40, 40, 48));
}

/* A city bus driving to the right; road_y is the top of the road. */
void city_bus(int x, int road_y, int anim)
{
    int y = road_y + 1, i;
    int bob = anim ? ((anim / 6) % 2) : 0;
    uint32_t body = RGB(44, 136, 146), roof = RGB(74, 172, 178), dark = RGB(30, 92, 100);

    y -= bob;
    gfx_rect(x, y, 78, 17, body);
    gfx_rect(x + 2, y - 2, 74, 2, roof);
    gfx_rect(x, y + 12, 78, 2, RGB(232, 222, 196));           /* stripe */
    gfx_rect(x, y + 15, 78, 2, dark);
    for (i = 0; i < 6; i++) {                                 /* passenger windows */
        int wx = x + 4 + i * 10;
        gfx_rect(wx, y + 2, 8, 7, RGB(255, 222, 150));
        gfx_blend_rect(wx, y + 2, 8, 3, RGB(255, 255, 255), 50);
        if (i == 2) gfx_rect(wx + 2, y + 5, 2, 4, RGB(60, 50, 60)); /* passenger */
        if (i == 4) gfx_rect(wx + 4, y + 4, 2, 5, RGB(60, 50, 60));
    }
    gfx_rect(x + 66, y + 2, 9, 9, RGB(150, 200, 230));        /* windshield */
    gfx_rect(x + 67, y - 1, 8, 3, RGB(20, 22, 30));           /* route sign */
    gfx_rect(x + 69, y, 4, 1, RGB(255, 170, 60));
    gfx_rect(x + 76, y + 11, 2, 2, RGB(255, 240, 200));       /* headlight */
    city_glow(x + 82, y + 12, 10, RGB(255, 240, 200), 60);
    gfx_rect(x, y + 11, 1, 2, RGB(255, 60, 60));              /* tail light */
    gfx_rect(x + 56, y + 3, 8, 12, dark);                     /* door */
    gfx_rect(x + 59, y + 3, 1, 12, RGB(20, 60, 66));
    wheel(x + 15, road_y + 18, anim);
    wheel(x + 62, road_y + 18, anim);
}

void city_person(int x, int ground_y, int anim, uint32_t shirt)
{
    int step = (anim / 5) % 4;
    int y = ground_y - 9;
    gfx_rect(x, y, 2, 2, RGB(230, 186, 150));                 /* head */
    gfx_rect(x - 1, y + 2, 4, 4, shirt);                      /* body */
    if (step == 0 || step == 2) {
        gfx_rect(x, y + 6, 1, 3, RGB(40, 40, 60));
        gfx_rect(x + 1, y + 6, 1, 3, RGB(40, 40, 60));
    } else {
        gfx_rect(x - 1, y + 6, 1, 3, RGB(40, 40, 60));
        gfx_rect(x + 2, y + 6, 1, 3, RGB(40, 40, 60));
    }
}

/* t = frames since launch: a rocket rises, then bursts into a ring of sparks. */
static void spark_pos(int x, int y, float a, float sp, float tt, int *px, int *py)
{
    *px = x + (int)(cosf(a) * sp * tt * (1.0f - tt / 90.0f));
    *py = y + (int)(sinf(a) * sp * tt * (1.0f - tt / 90.0f) + 0.03f * tt * tt);
}

void city_firework(int x, int launch_y, int burst_y, int t, uint32_t color, uint32_t seed)
{
    const int rise = 14, life = 38;
    int i;
    if (t < 0) return;
    if (t < rise) {
        float f = (float)t / (float)rise;
        int y = launch_y + (int)((float)(burst_y - launch_y) * (1.0f - (1.0f - f) * (1.0f - f)));
        gfx_rect(x, y, 1, 3, RGB(255, 244, 210));
        gfx_blend_rect(x, y + 3, 1, 5, RGB(255, 190, 110), 110);
        if (t % 2) gfx_rect(x + (t % 3) - 1, y + 7, 1, 1, RGB(255, 200, 120));
        return;
    }
    t -= rise;
    if (t > life) return;
    if (t < 5) city_glow(x, burst_y, 26 - t * 4, color, 150);
    for (i = 0; i < 34; i++) {
        float a = (float)i * 6.2832f / 34.0f + (float)(seed % 100) * 0.01f;
        float sp = 2.6f + (float)(hash3(seed, (uint32_t)i, 3) % 90) / 100.0f;
        int fade = 255 - t * 255 / life;
        int px, py, qx, qy, k;
        if (t > 22 && (hash3(seed, (uint32_t)i, (uint32_t)t) & 1)) continue;   /* crackle */
        for (k = 3; k >= 1; k--) {                                             /* trail */
            if (t - k < 0) continue;
            spark_pos(x, burst_y, a, sp, (float)(t - k), &qx, &qy);
            gfx_blend_rect(qx, qy, 1, 1, color, fade / (k + 1));
        }
        spark_pos(x, burst_y, a, sp, (float)t, &px, &py);
        if (t < 22) {
            gfx_blend_rect(px - 1, py, 3, 2, color, fade);
            gfx_blend_rect(px, py - 1, 1, 4, color, fade);
            if (t < 10) gfx_blend_rect(px, py, 1, 1, RGB(255, 255, 250), fade);
        } else {
            gfx_blend_rect(px, py, 2, 2, color, fade);
        }
    }
}

/* ------------------------------------------------------------------ */
/* isometric (year view)                                                */
/* ------------------------------------------------------------------ */

void city_iso_tile(int cx, int cy, int hw, int hh, uint32_t color)
{
    int x;
    for (x = -hw; x <= hw; x++) {
        int ax = x < 0 ? -x : x;
        int span = hh - (ax * hh) / hw;
        gfx_rect(cx + x, cy - span, 1, 2 * span + 1, color);
    }
}

void city_iso_block(int cx, int cy, int hw, int hh, int height,
                    uint32_t top, uint32_t left, uint32_t right)
{
    int x;
    for (x = cx - hw; x < cx; x++) {
        int yb = cy + ((x - (cx - hw)) * hh) / hw;
        gfx_rect(x, yb - height, 1, height + 1, left);
    }
    for (x = cx; x <= cx + hw; x++) {
        int yb = cy + hh - ((x - cx) * hh) / hw;
        gfx_rect(x, yb - height, 1, height + 1, right);
    }
    city_iso_tile(cx, cy - height, hw, hh, top);
}

/* ------------------------------------------------------------------ */
/* towers from the pixel-art pack                                       */
/* ------------------------------------------------------------------ */

static const TowerKit KITS[KIT_COUNT] = {
    { &SPR_BROWN_CAP, &SPR_BROWN_MID, &SPR_BROWN_BASE },
    { &SPR_SHOP_CAP,  &SPR_SHOP_MID,  &SPR_SHOP_BASE },
    { &SPR_CLUB_CAP,  &SPR_CLUB_MID,  &SPR_CLUB_BASE },
};
static const float KIT_HUE[KIT_COUNT] = { 4.0f, 19.0f, 5.0f };   /* hue of each style's wall */

/* season palettes for the kit sprites: [style][cap, mid, base][tier] */
static uint32_t g_skin_pal[KIT_COUNT][3][SEASON_TIERS_CITY][256];

/* window rectangles of each style's floor (from the glass mask) */
typedef struct { int x, y, w, h; } WinRect;
static WinRect g_win[KIT_COUNT][6];
static int g_win_n[KIT_COUNT];

static void rgb_to_hsv(uint32_t c, float *h, float *s, float *v)
{
    float r = (c & 0xFF) / 255.0f, g = ((c >> 8) & 0xFF) / 255.0f, b = ((c >> 16) & 0xFF) / 255.0f;
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;
    *v = mx;
    *s = mx > 0 ? d / mx : 0;
    if (d <= 0) { *h = 0; return; }
    if (mx == r)      *h = 60.0f * fmodf((g - b) / d, 6.0f);
    else if (mx == g) *h = 60.0f * ((b - r) / d + 2.0f);
    else              *h = 60.0f * ((r - g) / d + 4.0f);
    if (*h < 0) *h += 360.0f;
}

static uint32_t hsv_to_rgb(float h, float s, float v, uint32_t alpha)
{
    float c = v * s, x, m = v - c, r = 0, g = 0, b = 0;
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    if (h < 60)       { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return alpha | ((uint32_t)((b + m) * 255.0f + 0.5f) << 16) |
           ((uint32_t)((g + m) * 255.0f + 0.5f) << 8) | (uint32_t)((r + m) * 255.0f + 0.5f);
}

uint32_t city_skin_color(uint32_t color, int skin, float base_hue)
{
    /* target wall hue, saturation and brightness multipliers per tier */
    static const float hue[5] = { 0, 12.0f, 158.0f, 44.0f, 288.0f };
    static const float sat[5] = { 1, 1.30f, 1.05f, 1.45f, 1.70f };
    static const float val[5] = { 1, 1.06f, 1.06f, 1.14f, 1.00f };
    float h, s, v;
    if (skin <= 0 || skin >= 5) return color;
    rgb_to_hsv(color, &h, &s, &v);
    if (s < 0.12f) return color;              /* keep whites, greys and outlines */
    h += hue[skin] - base_hue;
    s *= sat[skin]; if (s > 1) s = 1;
    v *= val[skin]; if (v > 1) v = 1;
    return hsv_to_rgb(h, s, v, color & 0xFF000000u);
}

void city_init(void)
{
    int k, part, t, i;
    for (k = 0; k < KIT_COUNT; k++) {
        const Sprite *parts[3] = { KITS[k].cap, KITS[k].mid, KITS[k].base };
        const Sprite *m = KITS[k].mid;
        int in = 0, x, y;
        for (part = 0; part < 3; part++)
            for (t = 0; t < SEASON_TIERS_CITY; t++)
                for (i = 0; i < 256; i++)
                    g_skin_pal[k][part][t][i] = (i == 0) ? 0 : city_skin_color(parts[part]->pal[i], t, KIT_HUE[k]);

        /* windows: runs of columns that contain glass */
        g_win_n[k] = 0;
        for (x = 0; x <= m->w; x++) {
            int has = 0;
            for (y = 0; x < m->w && y < m->h; y++) if (m->glass[y * m->w + x]) { has = 1; break; }
            if (has && !in) { g_win[k][g_win_n[k]].x = x; in = 1; }
            if (!has && in && g_win_n[k] < 6) {
                WinRect *r = &g_win[k][g_win_n[k]];
                int top = m->h, bot = -1, xx;
                r->w = x - r->x;
                for (xx = r->x; xx < x; xx++)
                    for (y = 0; y < m->h; y++)
                        if (m->glass[y * m->w + xx]) { if (y < top) top = y; if (y > bot) bot = y; }
                r->y = top; r->h = bot - top + 1;
                g_win_n[k]++;
                in = 0;
            }
        }
    }
}

const TowerKit *city_kit(int style)
{
    if (style < 0 || style >= KIT_COUNT) style = 0;
    return &KITS[style];
}

static int kit_index(const TowerKit *k)
{
    int i;
    for (i = 0; i < KIT_COUNT; i++) if (k == &KITS[i]) return i;
    return 0;
}

int city_kit_height(const TowerKit *k, int floors, int roof)
{
    return k->base->h + floors * k->mid->h + (roof ? k->cap->h : 0);
}

static void draw_flag(int pole_x, int base_y, int anim)
{
    int r;
    gfx_rect(pole_x, base_y - 22, 1, 22, RGB(190, 190, 200));
    gfx_rect(pole_x - 1, base_y - 23, 3, 1, RGB(230, 200, 90));
    for (r = 0; r < 8; r++) {
        int shift = (((anim / 5) + r) % 3 == 0);
        uint32_t col = (r >= 3 && r <= 4) ? RGB(234, 88, 66) : RGB(240, 240, 246);
        gfx_rect(pole_x + 1 + shift, base_y - 22 + r, 13 - shift, 1, col);
    }
}

/* a resident looking out of window `w` of a floor whose top is at fy */
static void draw_resident(int ki, int x, int fy, int w, int who, int peek)
{
    const WinRect *r = &g_win[ki][w];
    const Sprite *s = RESIDENTS[who % RESIDENT_COUNT];
    int wx = x + r->x, wy = fy + r->y;
    if (peek <= 0) return;
    if (peek > r->h) peek = r->h;
    gfx_push_clip(wx, wy, r->w, r->h);
    gfx_sprite(s, wx + (r->w - s->w) / 2, wy + r->h - peek, 0);
    gfx_pop_clip();
}

void city_kit_tower(const TowerKit *k, int x, int ground_y, int floors,
                    const uint8_t *hours, int roof, int top_alpha, int anim,
                    int skin, uint32_t seed)
{
    int ki = kit_index(k), i, j;
    int sk = (skin > 0 && skin < SEASON_TIERS_CITY) ? skin : 0;
    const uint32_t *pal_cap  = sk ? g_skin_pal[ki][0][sk] : 0;
    const uint32_t *pal_mid  = sk ? g_skin_pal[ki][1][sk] : 0;
    const uint32_t *pal_base = sk ? g_skin_pal[ki][2][sk] : 0;
    int y = ground_y - k->base->h;

    gfx_sprite_pal(k->base, pal_base, x, y, 0, 0, 0);
    for (i = 0; i < floors; i++) {
        int hour = hours ? hours[i < 24 ? i : 23] : -1;
        int a = (i == floors - 1 && top_alpha >= 0) ? top_alpha : 150;
        uint32_t glass = city_window_color(hour);
        if (sk == 4) glass = (i % 2) ? RGB(80, 240, 255) : RGB(255, 90, 220);   /* neon */
        y -= k->mid->h;
        if (y > SCREEN_H || y + k->mid->h < 0) continue;
        if (a > 0) gfx_sprite_pal(k->mid, pal_mid, x, y, 0, glass, a);
        else       gfx_sprite_pal(k->mid, pal_mid, x, y, 0, RGB(16, 18, 34), 190);

        if (!seed) continue;
        /* residents: each lit window sometimes has someone looking out */
        if (i == floors - 1 && top_alpha >= 40 && top_alpha < 150) {
            draw_resident(ki, x, y, g_win_n[ki] / 2, (int)(seed + i), (top_alpha - 40) / 3);
            continue;   /* the new neighbour waves from the floor that just landed */
        }
        for (j = 0; j < g_win_n[ki]; j++) {
            uint32_t h = hash3(seed, (uint32_t)(i * 8 + j), 77);
            int t, peek;
            if (h % 3) continue;
            t = (anim + (int)(h >> 8) % 600) % 600;
            if (t >= 200) continue;
            peek = t < 20 ? t : (t > 180 ? 200 - t : 20);
            draw_resident(ki, x, y, j, (int)(h >> 4), peek);
        }
    }
    if (roof) {
        y -= k->cap->h;
        gfx_sprite_pal(k->cap, pal_cap, x, y, 0, 0, 0);
        draw_flag(x + k->cap->w / 2, y, anim);
    }
}

void city_day_sky(int y_top, int y_bottom)
{
    const Sprite *s = &SPR_DAY_SKY;
    int top = y_bottom + 32 - s->h;   /* its grass strip stays hidden below the street */
    if (top > y_top) gfx_rect(0, y_top, SCREEN_W, top - y_top, s->pal[s->px[0]]);
    gfx_push_clip(0, y_top, SCREEN_W, y_bottom - y_top);
    gfx_sprite(s, 0, top, 0);
    gfx_pop_clip();
}

/* A floor (or the cornice) hanging from the crane; windows are still dark. */
void city_kit_module(const TowerKit *k, int x, int bottom_y, int roof_module, int anim, int skin)
{
    int ki = kit_index(k);
    int sk = (skin > 0 && skin < SEASON_TIERS_CITY) ? skin : 0;
    if (roof_module) {
        gfx_sprite_pal(k->cap, sk ? g_skin_pal[ki][0][sk] : 0, x, bottom_y - k->cap->h, 0, 0, 0);
        draw_flag(x + k->cap->w / 2, bottom_y - k->cap->h, anim);
    } else {
        gfx_sprite_pal(k->mid, sk ? g_skin_pal[ki][1][sk] : 0, x, bottom_y - k->mid->h, 0,
                       RGB(16, 18, 34), 190);
    }
}

/* Crane jib across the top of the view, with the trolley, cable and hook. */
void city_crane_jib(int y, int x0, int x1, int hook_x, int hook_y, int sling_w, int anim)
{
    int x;
    gfx_rect(x0, y, x1 - x0, 2, C_CRANE);
    gfx_rect(x0, y + 7, x1 - x0, 2, C_CRANE);
    for (x = x0; x < x1; x += 8) {
        city_line(x, y + 1, x + 4, y + 7, C_CRANE);
        city_line(x + 4, y + 7, x + 8, y + 1, C_CRANE);
    }
    if ((anim / 14) % 2 == 0) gfx_rect(x1 - 2, y - 2, 2, 2, RGB(255, 70, 60));
    gfx_rect(hook_x - 4, y + 9, 9, 3, C_METAL);                        /* trolley */
    gfx_rect(hook_x, y + 12, 1, hook_y - y - 18, RGB(200, 200, 210));  /* cable */
    gfx_rect(hook_x - 2, hook_y - 7, 5, 3, C_CRANE);                  /* hook block */
    gfx_rect(hook_x, hook_y - 4, 1, 3, C_METAL);
    if (sling_w > 0) {                                                 /* slings */
        city_line(hook_x, hook_y - 2, hook_x - sling_w / 2, hook_y + 6, RGB(170, 170, 180));
        city_line(hook_x, hook_y - 2, hook_x + sling_w / 2, hook_y + 6, RGB(170, 170, 180));
    }
}

/* Dust puff when a floor lands; t = frames since landing. */
void city_dust(int x, int y, int t)
{
    int i;
    if (t < 0 || t > 20) return;
    for (i = 0; i < 6; i++) {
        int dx = (i % 2 ? 1 : -1) * (4 + t * (2 + i) / 3);
        int dy = -(t * (1 + i % 3)) / 4;
        int r = 3 + t / 4;
        city_glow(x + dx, y + dy, r, RGB(214, 196, 176), 150 - t * 7);
    }
}
