/*
 * city.h - pixel-art drawing for the "build a tower" views.
 * One completed focus session = one floor. The goal floor gets the roof.
 *
 * Everything takes an `anim` frame counter (30 per second) so the city
 * is alive: cranes work, buses drive by, lights flicker, flags wave.
 */
#ifndef POMO_CITY_H
#define POMO_CITY_H

#include <stdint.h>
#include "gfx.h"

typedef struct {
    int x, ground_y;        /* left edge and ground line */
    int w, floor_h;         /* building width, height of one floor */
    int floors;             /* completed floors */
    int goal;               /* floor that gets the roof (16) */
    const uint8_t *hours;   /* hour (0-23) each floor was completed, or NULL */
    int building;           /* 1 = a floor is under construction */
    float progress;         /* 0..1 progress of that floor */
    int windows;            /* windows per floor */
    int highlight;          /* floor index to highlight (just built), -1 = none */
    int crane;              /* draw a crane while building */
    uint32_t seed;          /* varies which windows are dark */
    /* animation; leaving these at 0 gives the static look */
    int anim;               /* frame counter */
    int reveal;             /* 0 = all lit; k > 0 = only k-1 windows lit on the top floor */
    int roof_drop;          /* roof drawn this many px above its place (falling in) */
    int flag_pct;           /* 0 = flag up; 1..100 = flag being raised */
    int style;              /* 0 = default colors, 1 + KIT_* = pack style colors */
    int skin;               /* monthly palette tier, 0 = classic */
} Tower;

/* sky and background */
void     city_sky(int top, int bottom);
void     city_sky_rect(int x, int y, int w, int h);
void     city_sky_time(int top, int bottom, int minute_of_day, int anim);
void     city_clouds(int top, int bottom, int anim);
void     city_skyline(int ground_y, int min_h, int max_h, uint32_t body,
                      uint32_t window, uint32_t seed);
void     city_ground(int ground_y, int bottom);
void     city_street(int sidewalk_y, int road_bottom, int anim);

/* buildings */
void     city_tower(const Tower *t);
void     city_lot(int x, int ground_y, int w, int future);
uint32_t city_window_color(int hour);

/* street life */
void     city_lamp(int x, int ground_y, int anim, uint32_t seed);
void     city_tree(int x, int ground_y, int variant, int anim);
void     city_bus_stop(int x, int ground_y);
void     city_bus(int x, int road_y, int anim);
void     city_person(int x, int ground_y, int anim, uint32_t shirt);
void     city_firework(int x, int launch_y, int burst_y, int t, uint32_t color, uint32_t seed);
void     city_glow(int cx, int cy, int r, uint32_t color, int alpha);

/* towers built from the pixel-art pack: base + N floors + cornice */
typedef struct { const Sprite *cap, *mid, *base; } TowerKit;
enum { KIT_BROWN = 0, KIT_SHOP, KIT_CLUB, KIT_COUNT };
void     city_init(void);   /* builds the season palettes and window maps */
const TowerKit *city_kit(int style);
int      city_kit_height(const TowerKit *k, int floors, int roof);
/* top_alpha: light level of the top floor's windows (0..255), -1 = normal.
   skin: season palette (0 = classic). seed != 0 lets residents look out of
   the windows (use the day number). */
void     city_kit_tower(const TowerKit *k, int x, int ground_y, int floors,
                        const uint8_t *hours, int roof, int top_alpha, int anim,
                        int skin, uint32_t seed);
void     city_kit_module(const TowerKit *k, int x, int bottom_y, int roof_module, int anim, int skin);
/* season palettes: recolors a color for tier `skin` (base_hue: the wall's hue) */
uint32_t city_skin_color(uint32_t color, int skin, float base_hue);
/* the City Mega Pack day sky, its bottom (far city) at y_bottom */
void     city_day_sky(int y_top, int y_bottom);
void     city_crane_jib(int y, int x0, int x1, int hook_x, int hook_y, int sling_w, int anim);
void     city_dust(int x, int y, int t);

/* primitives */
void     city_line(int x0, int y0, int x1, int y1, uint32_t color);
void     city_disc(int cx, int cy, int r, uint32_t color);
void     city_iso_block(int cx, int cy, int hw, int hh, int height,
                        uint32_t top, uint32_t left, uint32_t right);
void     city_iso_tile(int cx, int cy, int hw, int hh, uint32_t color);

#endif
