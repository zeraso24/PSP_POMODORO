/*
 * views.c - the city: Today (close-up with the crane), Week (a street),
 * Month (a skyline) and Year (an isometric city), plus the timer's window.
 *
 * Building art: "2D Pixel City Pack" by laavis (CC-BY 4.0).
 */
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "app.h"
#include "gfx.h"
#include "city.h"
#include "assets.h"
#include "datetime.h"

#define COL_BG     RGB(18, 18, 24)
#define COL_PANEL  RGB(38, 38, 50)
#define COL_TEXT   RGB(238, 238, 244)
#define COL_DIM    RGB(150, 150, 168)
#define COL_FOCUS  RGB(234, 88, 66)
#define COL_SCAFF  RGB(214, 170, 70)
#define COL_UNLOCK RGB(255, 170, 90)

#define GROUND0  236     /* Today view: ground line when the camera is at street level */
#define TOWER_X  48

/* celebration timeline, in animation ticks (30 per second) */
#define CEL_LAND        14
#define CEL_ROOF_START  30
#define CEL_ROOF_LAND   58
#define CEL_FIREWORKS   64
#define CEL_PAN_START  140
#define CEL_PAN_END    196
#define CEL_END        200

static const char *STYLE_NAMES[KIT_COUNT] = { "brownstone", "shop", "club" };

int views_style_for_day(int32_t day)
{
    int s = (int)(day % KIT_COUNT);
    return s < 0 ? s + KIT_COUNT : s;
}

static float smoothf(float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t * t * (3 - 2 * t);
}

static void fmt_duration(char *buf, size_t n, uint32_t secs)
{
    uint32_t h = secs / 3600, m = (secs / 60) % 60;
    if (h > 0) snprintf(buf, n, "%uh %02um", (unsigned)h, (unsigned)m);
    else       snprintf(buf, n, "%um", (unsigned)m);
}

static void fmt_date(char *buf, size_t n, int32_t day)
{
    int y, m, d, i;
    char wd[4], mo[4];
    dt_from_day_number(day, &y, &m, &d);
    for (i = 0; i < 3; i++) {
        char c = dt_weekday_name(day)[i];
        wd[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        c = dt_month_name(m)[i];
        mo[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    wd[3] = mo[3] = '\0';
    snprintf(buf, n, "%s %d %s", wd, d, mo);
}

static void copy_hours(const DayRec *r, uint8_t *out)
{
    if (r) memcpy(out, r->hour, DAY_HOURS);
    else   memset(out, HOUR_UNKNOWN, DAY_HOURS);
}

/* ------------------------------------------------------------------ */
/* shared pieces                                                        */
/* ------------------------------------------------------------------ */

static void sky(App *a, int y0, int y1)
{
    /* the pack's painted sky for dusk, a computed sky for other hours */
    if (a->minute_of_day >= 17 * 60 && a->minute_of_day < 20 * 60 + 30) {
        int y;
        for (y = y0; y < y1; y++)
            gfx_rect(0, y, SCREEN_W, 1, SKY_GRADIENT[(y - y0) * SKY_GRADIENT_H / (y1 - y0)]);
    } else {
        city_sky_time(y0, y1, a->minute_of_day, a->anim);
    }
}

static void skyline_layer(const Sprite *s, int bottom, int offset)
{
    int x;
    for (x = -(offset % s->w) - s->w; x < SCREEN_W; x += s->w)
        gfx_sprite(s, x, bottom - s->h, 0);
}

static void header(const char *title, int sel)
{
    static const char *names[VIEW_COUNT] = { "TODAY", "WEEK", "MONTH", "YEAR" };
    int x = 464, xs[VIEW_COUNT], i;
    gfx_blend_rect(0, 0, SCREEN_W, 30, RGB(18, 16, 30), 170);
    gfx_text(16, 9, 2, COL_TEXT, title);
    for (i = VIEW_COUNT - 1; i >= 0; i--) {
        x -= gfx_text_width(names[i], 1) + 12;
        xs[i] = x;
        x -= 4;
    }
    for (i = 0; i < VIEW_COUNT; i++) {
        int w = gfx_text_width(names[i], 1) + 12;
        if (i == sel) {
            gfx_rect(xs[i], 8, w, 14, COL_PANEL);
            gfx_rect(xs[i], 21, w, 2, COL_FOCUS);
        }
        gfx_text(xs[i] + 6, 11, 1, i == sel ? COL_TEXT : COL_DIM, names[i]);
    }
}

static int help_key(int x, const char *key, const char *label)
{
    x = gfx_text(x, 256, 1, COL_TEXT, key);
    x = gfx_text(x + 6, 256, 1, COL_DIM, label);
    return x + 14;
}

static void help_bar(const char *nav_key, const char *nav_label,
                     const char *extra_key, const char *extra_label)
{
    int x = 12;
    gfx_rect(0, 248, SCREEN_W, 24, COL_BG);
    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    x = help_key(x, "L/R", "View");
    x = help_key(x, nav_key, nav_label);
    if (extra_key) x = help_key(x, extra_key, extra_label);
    x = gfx_icon(x, 256, 1, ICON_CIRCLE);
    gfx_text(x + 4, 256, 1, COL_DIM, "Timer");
}

static void floor_bar(int x, int y, int floors, int goal, const uint8_t *hrs, float prog)
{
    int groups = (goal - 1) / 4;
    int seg = (224 - groups * 4) / goal;
    int i;
    if (seg < 3) seg = 3;
    for (i = 0; i < goal; i++) {
        int sx = x + i * seg + (i / 4) * 4;
        uint32_t c = RGB(60, 56, 76);
        if (i < floors) c = city_window_color(hrs[i < DAY_HOURS ? i : DAY_HOURS - 1]);
        gfx_rect(sx, y, seg - 2, 8, c);
        if (i == floors && prog > 0) gfx_rect(sx, y, (int)((seg - 2) * prog), 8, COL_SCAFF);
    }
}

/* the pack's bus with rolling wheels; x = left edge, ground = road line */
static void pack_bus(int x, int ground, int anim)
{
    static const Sprite *tires[4] = { &SPR_TIRE, &SPR_TIRE_1, &SPR_TIRE_2, &SPR_TIRE_3 };
    const Sprite *t = tires[(anim / 2) % 4];
    int by = ground + 6 - SPR_BUS.h;
    if (x < -SPR_BUS.w || x > SCREEN_W) return;
    gfx_sprite(&SPR_BUS, x, by, 0);
    gfx_sprite(t, x + 52, by + 62, 0);
    gfx_sprite(t, x + 165, by + 62, 0);
}

/* ------------------------------------------------------------------ */
/* the tower close-up (Today view and timer window)                     */
/* ------------------------------------------------------------------ */

typedef struct {
    int shown;          /* floors drawn (capped at the goal) */
    int roof;           /* draw the cornice and flag */
    int top_alpha;      /* light level of the top floor windows, -1 = normal */
    int module;         /* 0 none, 1 floor hanging, 2 roof hanging */
    int gap;            /* distance between the hanging piece and the tower */
    int sway;
    int hook_y_rise;    /* >0: empty hook rising after a landing (ticks) */
    int dust_t;         /* ticks since landing, -1 = none */
} Build;

/* What the crane is doing for `day` right now. */
static Build build_state(App *a, int32_t day)
{
    Pomo *p = &a->p;
    Build b;
    int goal = (int)p->set.daily_goal;
    int floors = pomo_floors(p, day);
    int is_today = (day == a->today);
    memset(&b, 0, sizeof(b));
    b.shown = floors < goal ? floors : goal;
    b.roof = floors >= goal;
    b.top_alpha = -1;
    b.dust_t = -1;

    if (is_today && a->celebrate >= 0) {
        int t = a->celebrate;
        b.roof = t >= CEL_ROOF_LAND;
        if (t < CEL_LAND) {
            b.shown = goal - 1;
            b.module = 1;
            b.gap = (int)(14.0f * (1.0f - smoothf((float)t / CEL_LAND)));
        } else {
            b.shown = goal;
            if (t < CEL_LAND + 16) b.top_alpha = (t - CEL_LAND) * 10;
            if (t < CEL_LAND + 20) b.dust_t = t - CEL_LAND;
        }
        if (t >= CEL_ROOF_START && t < CEL_ROOF_LAND) {
            b.module = 2;
            b.gap = (int)(40.0f * (1.0f - smoothf((float)(t - CEL_ROOF_START) / (CEL_ROOF_LAND - CEL_ROOF_START))));
        }
        if (t >= CEL_ROOF_LAND && t < CEL_ROOF_LAND + 20) b.dust_t = t - CEL_ROOF_LAND;
        if (t >= CEL_ROOF_LAND && t < CEL_ROOF_LAND + 30) b.hook_y_rise = t - CEL_ROOF_LAND + 1;
        return b;
    }

    if (is_today && a->land_t >= 0 && a->land_t < 45 && floors > 0) {
        if (a->land_t < 16) b.top_alpha = a->land_t * 10;
        if (a->land_t < 20) b.dust_t = a->land_t;
        b.hook_y_rise = a->land_t + 1;
        return b;
    }

    if (is_today && !b.roof && p->phase == PH_FOCUS) {
        b.module = 1;
        b.gap = (int)(34.0f * (1.0f - pomo_progress(p)));
        if (p->running) b.sway = (int)(2.5f * sinf(a->anim * 0.07f));
    }
    return b;
}

/* Draws the tower whose ground line is at `ground`; returns the top y. */
static int draw_build(App *a, int32_t day, const Build *b, int x, int ground, int jib_y, int jib_x1)
{
    const TowerKit *k = city_kit(views_style_for_day(day));
    const DayRec *r = pomo_find_day(&a->p, day);
    uint8_t hrs[DAY_HOURS];
    int top;
    int hook_x = x + k->mid->w / 2;

    copy_hours(r, hrs);
    city_kit_tower(k, x, ground, b->shown, hrs, b->roof, b->top_alpha, a->anim);
    top = ground - city_kit_height(k, b->shown, 0);

    /* bonus floors past the goal: blinking lights on an antenna */
    if (b->roof && r && (int)r->sessions > (int)a->p.set.daily_goal && a->celebrate < 0) {
        int extra = (int)r->sessions - (int)a->p.set.daily_goal, i;
        int ax = x + k->cap->w - 18, ay = top - k->cap->h;
        int h = 10 + extra * 6;
        gfx_rect(ax, ay - h, 1, h, RGB(190, 190, 200));
        for (i = 0; i < extra && i < 8; i++)
            if (((a->anim / 12) + i) % 2 == 0)
                gfx_rect(ax - 1, ay - h + i * 6, 3, 2, RGB(255, 60, 60));
    }

    if (b->module == 1) {
        int mb = top - b->gap;
        city_kit_module(k, x + b->sway, mb, 0, a->anim);
        city_crane_jib(jib_y, -10, jib_x1, hook_x + b->sway, mb - k->mid->h, 110, a->anim);
    } else if (b->module == 2) {
        int mb = top - b->gap;
        city_kit_module(k, x, mb, 1, a->anim);
        city_crane_jib(jib_y, -10, jib_x1, hook_x, mb - k->cap->h - 26, 0, a->anim);
    } else if (b->hook_y_rise > 0) {
        int hy = top - (b->roof ? 40 : 6) - b->hook_y_rise * 3;
        if (hy > jib_y + 20) city_crane_jib(jib_y, -10, jib_x1, hook_x, hy, 0, a->anim);
    }
    if (b->dust_t >= 0) {
        int dy = top + (b->roof ? 0 : k->mid->h) - 2;
        city_dust(x + 4, dy, b->dust_t);
        city_dust(x + k->mid->w - 4, dy, b->dust_t);
    }
    return top;
}

void views_timer_window(App *a, int wx, int wy, int ww, int wh)
{
    const TowerKit *k = city_kit(views_style_for_day(a->today));
    Build b = build_state(a, a->today);
    int tx = wx + (ww - k->mid->w) / 2;
    int target = wy + (b.module ? 150 : 118);
    int height = city_kit_height(k, b.shown, 0);
    int ground = target + height;
    int street_y = wy + wh - 18;
    char buf[32];

    if (ground < street_y) ground = street_y;       /* short tower: show the street */

    gfx_set_clip(wx, wy, ww, wh);
    sky(a, wy, wy + wh);
    gfx_sprite(&SPR_SKYLINE_FAR, wx - 120, ground + 40 - SPR_SKYLINE_FAR.h + (ground - street_y) / 6, 0);
    gfx_rect(wx, ground, ww, 20, RGB(120, 108, 120));
    gfx_rect(wx, ground, ww, 1, RGB(160, 146, 156));
    draw_build(a, a->today, &b, tx, ground, wy + 4, wx + ww + 4);
    gfx_blend_rect(wx, wy + wh - 16, ww, 16, RGB(10, 10, 18), 190);
    snprintf(buf, sizeof(buf), "%d/%d FLOORS", pomo_floors(&a->p, a->today), (int)a->p.set.daily_goal);
    gfx_text(wx + (ww - gfx_text_width(buf, 1)) / 2, wy + wh - 12, 1, COL_TEXT, buf);
    gfx_reset_clip();
    gfx_frame(wx, wy, ww, wh, COL_PANEL);
}

/* ------------------------------------------------------------------ */
/* TODAY                                                                */
/* ------------------------------------------------------------------ */

static int today_cam_max(App *a, int32_t day)
{
    const TowerKit *k = city_kit(views_style_for_day(day));
    Build b = build_state(a, day);
    int h = city_kit_height(k, b.shown, b.roof);
    int target = (b.module || a->celebrate >= 0) ? 150 : 100;
    int cam = h + target - GROUND0;
    return cam > 0 ? cam : 0;
}

static void draw_today(App *a)
{
    Pomo *p = &a->p;
    int32_t day = a->sel_day;
    int is_today = (day == a->today);
    const DayRec *r = pomo_find_day(p, day);
    int floors = r ? (int)r->sessions : 0;
    int goal = (int)p->set.daily_goal;
    int style = views_style_for_day(day);
    Build b = build_state(a, day);
    int cam, ground, top, i, show_panel;
    int t = a->celebrate;
    char buf[64], dur[24];
    uint8_t hrs[DAY_HOURS];

    /* camera */
    if (is_today && t >= 0) {
        int top_cam = today_cam_max(a, day);
        cam = top_cam;
        if (t >= CEL_PAN_START)
            cam = (int)((float)top_cam * (1.0f - smoothf((float)(t - CEL_PAN_START) / (CEL_PAN_END - CEL_PAN_START))));
    } else if (a->cam_follow) {
        cam = today_cam_max(a, day);
    } else {
        int mx = today_cam_max(a, day);
        cam = a->cam > mx ? mx : a->cam;
    }
    a->cam = cam;
    ground = GROUND0 + cam;

    sky(a, 0, 248);
    if (cam > 120) {   /* clouds you pass on the way up */
        for (i = 0; i < 5; i++) {
            int wy = 380 + i * 110;
            int x = (i * 137 + a->anim / 2) % 620 - 70;
            int y = ground - wy;
            gfx_blend_rect(x, y, 70, 5, RGB(255, 236, 240), 70);
            gfx_blend_rect(x + 12, y - 4, 44, 4, RGB(255, 236, 240), 60);
        }
    }

    /* fireworks during the celebration */
    if (is_today && t >= CEL_FIREWORKS) {
        static const uint32_t col[6] = { RGB(255, 60, 40), RGB(255, 236, 80), RGB(40, 200, 255),
                                         RGB(255, 70, 200), RGB(80, 255, 120), RGB(255, 255, 255) };
        static const int fx[6] = { 300, 410, 220, 360, 440, 260 };
        static const int fh[6] = { 60, 76, 40, 30, 52, 16 };
        int tower_top = ground - city_kit_height(city_kit(style), goal, 1);
        for (i = 0; i < 6; i++)
            city_firework(fx[i], tower_top + 80, tower_top - fh[i], t - CEL_FIREWORKS - i * 5,
                          col[i], 300 + (uint32_t)i);
    }

    skyline_layer(&SPR_SKYLINE_FAR, GROUND0 + (int)(cam * 0.12f) + 6, 40);
    skyline_layer(style == KIT_CLUB ? &SPR_SKYLINE_A : &SPR_SKYLINE_B, GROUND0 + (int)(cam * 0.3f) + 4, 120);

    /* street */
    if (ground < SCREEN_H) {
        gfx_rect(0, ground, SCREEN_W, 14, RGB(120, 108, 120));
        gfx_rect(0, ground, SCREEN_W, 1, RGB(160, 146, 156));
        for (i = 0; i < SCREEN_W; i += 16) gfx_rect(i, ground + 1, 1, 13, RGB(104, 94, 106));
    }

    top = draw_build(a, day, &b, TOWER_X, ground, 32, 250);
    (void)top;

    /* street props in front: what you've unlocked */
    if (ground - 130 < SCREEN_H) {
        if (p->unlocked & UNLOCK_TREE_GREEN)
            gfx_sprite(&SPR_TREE_GREEN, -10, ground - SPR_TREE_GREEN.h + 2, 0);
        if (p->unlocked & UNLOCK_TREE_ORANGE)
            gfx_sprite(&SPR_TREE_ORANGE, 186, ground - SPR_TREE_ORANGE.h + 2, 0);
        if (p->unlocked & UNLOCK_TREE_PURPLE)
            gfx_sprite(&SPR_TREE_PURPLE, 420, ground - SPR_TREE_PURPLE.h + 2, 0);
        gfx_sprite(&SPR_BUS_SIGN, 244, ground - SPR_BUS_SIGN.h + 2, 0);
        if (style == KIT_CLUB) gfx_sprite(&SPR_ROPE, TOWER_X + 90, ground - SPR_ROPE.h, 0);
        if (p->unlocked & UNLOCK_BUS)
            pack_bus(-240 + (a->anim % 600) * 6, ground, a->anim);
    }

    /* info panel (stepped aside during the celebration) */
    show_panel = !(is_today && t >= 0 && t < CEL_PAN_END);
    if (show_panel) {
        int e;
        copy_hours(r, hrs);
        gfx_blend_rect(222, 40, 250, 196, RGB(16, 14, 28), 175);
        gfx_text(234, 50, 1, COL_DIM, "FLOORS");
        snprintf(buf, sizeof(buf), "%d", floors);
        e = gfx_text(234, 62, 4, floors >= goal ? COL_FOCUS : COL_TEXT, buf);
        snprintf(buf, sizeof(buf), "/%d", goal);
        gfx_text(e + 4, 74, 2, COL_DIM, buf);
        floor_bar(234, 100, floors < goal ? floors : goal, goal, hrs,
                  (is_today && p->phase == PH_FOCUS && floors < goal) ? pomo_progress(p) : 0);

        if (floors >= goal) {
            gfx_text(234, 116, 2, (is_today && t >= 0 && (a->anim % 2)) ? COL_TEXT : COL_FOCUS, "DAY COMPLETE!");
            if (floors > goal) {
                snprintf(buf, sizeof(buf), "+%d bonus floor%s", floors - goal, floors - goal == 1 ? "" : "s");
                gfx_text(234, 136, 1, COL_TEXT, buf);
            }
        } else if (is_today && p->phase == PH_FOCUS && p->running) {
            int s = pomo_remaining_sec(p);
            snprintf(buf, sizeof(buf), "Crane lowering floor %d", floors + 1);
            gfx_text(234, 118, 1, COL_TEXT, buf);
            snprintf(buf, sizeof(buf), "%02d:%02d until it lands", s / 60, s % 60);
            gfx_text(234, 130, 1, COL_DIM, buf);
        } else if (is_today && p->phase == PH_FOCUS) {
            snprintf(buf, sizeof(buf), "Next: floor %d", floors + 1);
            gfx_text(234, 118, 1, COL_TEXT, buf);
            gfx_text(234, 130, 1, COL_DIM, "Start it from the timer");
        } else if (is_today) {
            gfx_text(234, 118, 1, COL_TEXT, "Break time");
            snprintf(buf, sizeof(buf), "%d floors to the roof", goal - floors);
            gfx_text(234, 130, 1, COL_DIM, buf);
        } else {
            snprintf(buf, sizeof(buf), floors ? "%d floors built" : "No floors this day", floors);
            gfx_text(234, 118, 1, COL_TEXT, buf);
        }

        gfx_text(234, 152, 1, COL_DIM, "FOCUS");
        fmt_duration(dur, sizeof(dur), r ? r->focus_sec : 0);
        gfx_text(234, 164, 3, COL_TEXT, dur);

        if (is_today) {
            uint32_t st = pomo_streak(p, a->today);
            snprintf(buf, sizeof(buf), "Streak: %u day%s", (unsigned)st, st == 1 ? "" : "s");
            gfx_text(234, 196, 1, COL_TEXT, buf);
        }
        if (a->unlock_t > 0 && a->new_unlocks) {
            const char *what = (a->new_unlocks & UNLOCK_BUS) ? "the city bus" :
                               (a->new_unlocks & UNLOCK_TREE_PURPLE) ? "purple tree" :
                               (a->new_unlocks & UNLOCK_TREE_ORANGE) ? "orange tree" : "green tree";
            snprintf(buf, sizeof(buf), "New: %s unlocked!", what);
            gfx_text(234, 210, 1, COL_UNLOCK, buf);
        } else {
            snprintf(buf, sizeof(buf), "Style: %s", STYLE_NAMES[style]);
            gfx_text(234, 210, 1, COL_DIM, buf);
        }
    }

    fmt_date(buf, sizeof(buf), day);
    header(buf, VIEW_TODAY);
    help_bar("LEFT/RIGHT", "Day", "UP/DOWN", "Look");
}

/* ------------------------------------------------------------------ */
/* WEEK                                                                 */
/* ------------------------------------------------------------------ */

static int small_floor_h(int goal, int room)
{
    int fh = room / goal;
    if (fh > 8) fh = 8;
    if (fh < 4) fh = 4;
    return fh;
}

static void draw_week(App *a)
{
    Pomo *p = &a->p;
    int ground = 196, slot = 62, x0 = 23, bw = 40;
    int goal = (int)p->set.daily_goal;
    int fh = small_floor_h(goal, 128);
    uint32_t total = 0, roofs = 0, focus = 0;
    char buf[64], dur[24];
    int i, x, y1, m1, d1, y2, m2, d2;
    uint8_t hrs[DAY_HOURS];

    sky(a, 0, ground);
    city_clouds(40, 110, a->anim);
    city_skyline(ground, 50, 100, RGB(44, 34, 74), RGB(120, 92, 120), 11);
    city_skyline(ground, 26, 70, RGB(30, 26, 54), RGB(200, 150, 100), 12);

    for (x = x0 + 4; x < x0 + 7 * slot; x += 4)
        gfx_rect(x, ground - goal * fh - 1, 2, 1, RGB(170, 70, 60));
    snprintf(buf, sizeof(buf), "%d", goal);
    gfx_text(x0 - 18, ground - goal * fh - 5, 1, RGB(170, 70, 60), buf);

    for (i = 0; i < 7; i++) {
        int32_t day = a->week_start + i;
        const DayRec *r = pomo_find_day(p, day);
        int n = r ? (int)r->sessions : 0;
        int bx = x0 + i * slot + (slot - bw) / 2;
        int building = (day == a->today && p->phase == PH_FOCUS && n < goal);
        Tower t;
        if (day > a->today) { city_lot(bx - 4, ground, bw + 8, 1); continue; }
        if (n == 0 && !building) { city_lot(bx, ground, bw, 0); continue; }
        copy_hours(r, hrs);
        memset(&t, 0, sizeof(t));
        t.x = bx; t.ground_y = ground; t.w = bw; t.floor_h = fh;
        t.floors = n; t.goal = goal; t.hours = hrs;
        t.building = building; t.progress = pomo_progress(p);
        t.windows = 3; t.highlight = -1; t.crane = building;
        t.seed = (uint32_t)day; t.anim = a->anim;
        t.style = 1 + views_style_for_day(day);
        city_tower(&t);
        {
            int shown = n < goal ? n : goal;
            int ty = ground - shown * fh - (n >= goal ? 30 : (building ? 50 : 14));
            snprintf(buf, sizeof(buf), "%d", n);
            gfx_text(bx + bw / 2 - gfx_text_width(buf, 1) / 2, ty, 1,
                     day == a->today ? RGB(255, 200, 150) : COL_TEXT, buf);
        }
        total += (uint32_t)n;
        if (n >= goal) roofs++;
        if (r) focus += r->focus_sec;
    }

    city_street(ground, 232, a->anim);
    for (i = 1; i < 7; i++) {
        int gx = x0 + i * slot;
        if (i % 2) {
            city_lamp(gx - 3, ground + 6, a->anim, (uint32_t)i * 5);
        } else {
            int v = i / 2 - 1;   /* 0 green, 1 orange, 2 purple */
            uint32_t need = v == 0 ? UNLOCK_TREE_GREEN : v == 1 ? UNLOCK_TREE_ORANGE : UNLOCK_TREE_PURPLE;
            if (p->unlocked & need) city_tree(gx - 6, ground + 6, v, a->anim);
            else city_lamp(gx - 3, ground + 6, a->anim, (uint32_t)i * 7);
        }
    }
    city_bus_stop(x0 + 5 * slot - 8, ground + 6);
    city_person((int)(a->anim * 0.6f) % 520 - 20, ground + 7, a->anim, RGB(220, 90, 90));
    city_person(500 - (int)(a->anim * 0.45f + 200) % 520, ground + 7, a->anim + 3, RGB(90, 170, 230));
    if (p->unlocked & UNLOCK_BUS)
        city_bus((a->anim * 3) % 640 - 110, ground + 9, a->anim);

    gfx_rect(0, 233, SCREEN_W, 15, RGB(16, 16, 22));
    for (i = 0; i < 7; i++) {
        int32_t day = a->week_start + i;
        int cx = x0 + i * slot + slot / 2, d;
        dt_from_day_number(day, 0, 0, &d);
        snprintf(buf, sizeof(buf), "%s %d", dt_weekday_name(day), d);
        gfx_text(cx - gfx_text_width(buf, 1) / 2, 237, 1, day == a->today ? COL_FOCUS : COL_DIM, buf);
    }

    dt_from_day_number(a->week_start, &y1, &m1, &d1);
    dt_from_day_number(a->week_start + 6, &y2, &m2, &d2);
    if (m1 == m2) snprintf(buf, sizeof(buf), "%s %d - %d", dt_month_name(m1), d1, d2);
    else snprintf(buf, sizeof(buf), "%s %d - %s %d", dt_month_name(m1), d1, dt_month_name(m2), d2);
    for (i = 0; buf[i]; i++) if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] = (char)(buf[i] - 32);
    header(buf, VIEW_WEEK);
    fmt_duration(dur, sizeof(dur), focus);
    snprintf(buf, sizeof(buf), "%u floors - %u roof%s - %s focus", (unsigned)total, (unsigned)roofs,
             roofs == 1 ? "" : "s", dur);
    gfx_text(16, 34, 1, RGB(220, 210, 230), buf);
    help_bar("LEFT/RIGHT", "Week", 0, 0);
}

/* ------------------------------------------------------------------ */
/* MONTH                                                                */
/* ------------------------------------------------------------------ */

static int days_in_month(int y, int m)
{
    int ny = m == 12 ? y + 1 : y, nm = m == 12 ? 1 : m + 1;
    return (int)(dt_day_number(ny, nm, 1) - dt_day_number(y, m, 1));
}

static void draw_month(App *a)
{
    Pomo *p = &a->p;
    int ground = 210, slot = 14, x0 = 26, bw = 11;
    int goal = (int)p->set.daily_goal;
    int fh = small_floor_h(goal, 128);
    int32_t first = dt_day_number(a->month_y, a->month_m, 1);
    int ndays = days_in_month(a->month_y, a->month_m);
    uint32_t floors = 0, roofs = 0;
    int best = 0, bestd = 0, elapsed = 0, i, x;
    char buf[96];
    static const char *MONTHS_UP[12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
    uint8_t hrs[DAY_HOURS];

    sky(a, 0, ground);
    city_skyline(ground, 40, 90, RGB(40, 32, 70), RGB(110, 90, 120), 31);
    gfx_rect(0, ground, SCREEN_W, 38, RGB(24, 24, 32));
    gfx_rect(0, ground, SCREEN_W, 2, RGB(78, 78, 96));
    for (x = x0 + 2; x < x0 + 31 * slot; x += 4)
        gfx_rect(x, ground - goal * fh - 1, 2, 1, RGB(170, 70, 60));
    snprintf(buf, sizeof(buf), "%d", goal);
    gfx_text(x0 - 22, ground - goal * fh - 5, 1, RGB(170, 70, 60), buf);

    for (i = 0; i < ndays; i++) {
        int32_t day = first + i;
        const DayRec *r = pomo_find_day(p, day);
        int n = r ? (int)r->sessions : 0;
        int bx = x0 + i * slot + (slot - bw) / 2;
        if (day > a->today) city_lot(bx, ground, bw, 1);
        else if (n == 0) city_lot(bx, ground, bw, 0);
        else {
            Tower t;
            copy_hours(r, hrs);
            memset(&t, 0, sizeof(t));
            t.x = bx; t.ground_y = ground; t.w = bw; t.floor_h = fh;
            t.floors = n; t.goal = goal; t.hours = hrs; t.windows = 2;
            t.highlight = -1; t.seed = (uint32_t)day; t.anim = a->anim;
            t.style = 1 + views_style_for_day(day);
            city_tower(&t);
        }
        if (day <= a->today) elapsed++;
        floors += (uint32_t)n;
        if (n >= goal) roofs++;
        if (n > best) { best = n; bestd = i + 1; }
        if (i == 0 || (i + 1) % 5 == 0) {
            int cx = x0 + i * slot + slot / 2;
            snprintf(buf, sizeof(buf), "%d", i + 1);
            gfx_text(cx - gfx_text_width(buf, 1) / 2, ground + 6, 1, day == a->today ? COL_FOCUS : COL_DIM, buf);
        }
        if (day == a->today)
            gfx_rect(x0 + i * slot + 2, ground + 16, slot - 4, 2, COL_FOCUS);
    }

    snprintf(buf, sizeof(buf), "%s %d", MONTHS_UP[a->month_m - 1], a->month_y);
    header(buf, VIEW_MONTH);
    snprintf(buf, sizeof(buf), "%u floors - %u roof%s - avg %.1f/day - best %d (day %d)",
             (unsigned)floors, (unsigned)roofs, roofs == 1 ? "" : "s",
             elapsed ? (float)floors / (float)elapsed : 0.0f, best, bestd);
    gfx_text(16, 34, 1, RGB(220, 210, 230), buf);
    help_bar("LEFT/RIGHT", "Month", 0, 0);
}

/* ------------------------------------------------------------------ */
/* YEAR                                                                 */
/* ------------------------------------------------------------------ */

static void block_colors(int n, int goal, uint32_t *top, uint32_t *left, uint32_t *right)
{
    int r, g, bl;
    float f = (float)n / (float)goal;
    if (f <= 0.25f)      { r = 70;  g = 80;  bl = 112; }
    else if (f <= 0.5f)  { r = 88;  g = 108; bl = 160; }
    else if (f <= 0.75f) { r = 108; g = 148; bl = 210; }
    else if (f < 1.0f)   { r = 150; g = 190; bl = 240; }
    else                 { r = 170; g = 200; bl = 246; }
    *top = RGB(r + 30 > 255 ? 255 : r + 30, g + 30 > 255 ? 255 : g + 30, bl + 9 > 255 ? 255 : bl + 9);
    *left = RGB(r, g, bl);
    *right = RGB(r * 6 / 10, g * 6 / 10, bl * 6 / 10);
    if (n >= goal) *top = RGB(234, 88, 66);
}

static void draw_year(App *a)
{
    Pomo *p = &a->p;
    const int ox = 82, oy = 72, hw = 6, hh = 3;
    int goal = (int)p->set.daily_goal;
    int year, mm, dd, s, c, i;
    int32_t jan1, ndays;
    int wd0;
    uint32_t total = 0, roofs = 0;
    char buf[64], dur[24];
    static const char *MON[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    const DayRec *sr;

    dt_from_day_number(a->sel_day, &year, &mm, &dd);
    jan1 = dt_day_number(year, 1, 1);
    ndays = dt_day_number(year + 1, 1, 1) - jan1;
    wd0 = dt_weekday(jan1);

    sky(a, 0, 248);

    for (s = 0; s <= 53 + 6; s++) {
        for (c = 0; c <= 53; c++) {
            int r = s - c, idx, n, cx, cy, m;
            int32_t day;
            if (r < 0 || r > 6) continue;
            idx = c * 7 + r - wd0;
            if (idx < 0 || idx >= ndays) continue;
            day = jan1 + idx;
            dt_from_day_number(day, 0, &m, 0);
            cx = ox + (c - r) * hw;
            cy = oy + (c + r) * hh;
            city_iso_tile(cx, cy, hw, hh, (m % 2) ? RGB(34, 34, 46) : RGB(42, 40, 56));
            n = pomo_floors(p, day);
            if (n > 0) {
                uint32_t t, l, rr;
                int hgt = (n > goal ? goal : n) * 32 / goal;
                block_colors(n, goal, &t, &l, &rr);
                if (hgt < 2) hgt = 2;
                city_iso_block(cx, cy, 4, 2, hgt, t, l, rr);
                if (n > goal) {
                    gfx_rect(cx, cy - hgt - 7, 1, 6, RGB(190, 190, 200));
                    gfx_rect(cx, cy - hgt - 8, 1, 1, RGB(255, 60, 60));
                }
            }
            if (day == a->sel_day) {   /* cursor */
                int top = cy - (n > 0 ? (n > goal ? goal : n) * 32 / goal : 0) - 14 - ((a->anim / 8) % 2);
                city_line(cx - 3, top, cx, top + 4, COL_TEXT);
                city_line(cx + 3, top, cx, top + 4, COL_TEXT);
                gfx_rect(cx - 3, top - 1, 7, 1, COL_TEXT);
            }
        }
    }

    /* month labels above the back edge */
    for (i = 1; i <= 12; i++) {
        int32_t d1 = dt_day_number(year, i, 1);
        int col = (int)(d1 - jan1 + wd0) / 7;
        int lx = ox + col * hw + 2, ly = oy + col * hh - 50;
        gfx_text(lx, ly, 1, COL_DIM, MON[i - 1]);
        gfx_rect(lx, ly + 10, 1, 3, RGB(70, 70, 90));
    }

    pomo_range_totals(p, jan1, jan1 + ndays - 1, &total, 0);
    for (i = 0; i < ndays; i++) if (pomo_floors(p, jan1 + i) >= goal) roofs++;
    snprintf(buf, sizeof(buf), "%u", (unsigned)total);
    c = gfx_text(300, 44, 3, COL_TEXT, buf);
    gfx_text(c + 6, 58, 1, COL_DIM, "floors");
    snprintf(buf, sizeof(buf), "%u roof%s", (unsigned)roofs, roofs == 1 ? "" : "s");
    gfx_text(300, 74, 1, COL_DIM, buf);
    snprintf(buf, sizeof(buf), "Best streak: %u days", (unsigned)pomo_best_streak(p));
    gfx_text(300, 86, 1, COL_DIM, buf);

    /* selected day */
    sr = pomo_find_day(p, a->sel_day);
    gfx_blend_rect(12, 168, 156, 76, RGB(30, 28, 44), 220);
    fmt_date(buf, sizeof(buf), a->sel_day);
    gfx_text(20, 174, 2, COL_TEXT, buf);
    if (a->sel_day > a->today) snprintf(buf, sizeof(buf), "Not here yet");
    else snprintf(buf, sizeof(buf), "%d floors%s", sr ? (int)sr->sessions : 0,
                  (sr && (int)sr->sessions >= goal) ? " - roof!" : "");
    gfx_text(20, 194, 1, COL_SCAFF, buf);
    fmt_duration(dur, sizeof(dur), sr ? sr->focus_sec : 0);
    snprintf(buf, sizeof(buf), "%s focus", dur);
    gfx_text(20, 206, 1, COL_DIM, buf);
    {
        static const float fr[5] = { 0.2f, 0.45f, 0.7f, 0.9f, 1.0f };
        static const char *lab[5] = { "low", "", "", "high", "roof" };
        int lx = 20;
        for (i = 0; i < 5; i++) {
            uint32_t t, l, rr;
            block_colors((int)(fr[i] * goal + 0.5f), goal, &t, &l, &rr);
            gfx_rect(lx, 224, 10, 7, i == 4 ? t : l);
            lx += 12;
            if (lab[i][0]) lx = gfx_text(lx, 224, 1, COL_DIM, lab[i]) + 6;
        }
    }

    snprintf(buf, sizeof(buf), "%d", year);
    header(buf, VIEW_YEAR);
    help_bar("PAD", "Move", 0, 0);
}

/* ------------------------------------------------------------------ */
/* navigation                                                           */
/* ------------------------------------------------------------------ */

void views_open(App *a, int view)
{
    int y, m, d;
    a->view = view;
    a->sel_day = a->today;
    a->week_start = a->today - dt_weekday(a->today);
    dt_from_day_number(a->today, &y, &m, &d);
    a->month_y = y;
    a->month_m = m;
    a->cam_follow = 1;
}

void views_start_celebration(App *a)
{
    a->screen = SCR_CITY;
    views_open(a, VIEW_TODAY);
    a->celebrate = 0;
}

int views_input(App *a, uint32_t cur, uint32_t pressed)
{
    (void)cur;
    if (a->celebrate >= 0) {   /* any button skips the show */
        if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START | PSP_CTRL_SELECT)) {
            a->celebrate = -1;
            a->cam_follow = 0;
            a->cam = 0;
            return 1;
        }
        return 0;
    }
    if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_SELECT)) {
        a->screen = SCR_TIMER;
        return 1;
    }
    if (pressed & PSP_CTRL_LTRIGGER) { a->view = (a->view + VIEW_COUNT - 1) % VIEW_COUNT; a->cam_follow = 1; return 1; }
    if (pressed & PSP_CTRL_RTRIGGER) { a->view = (a->view + 1) % VIEW_COUNT; a->cam_follow = 1; return 1; }

    switch (a->view) {
    case VIEW_TODAY:
        if (pressed & PSP_CTRL_LEFT)  { a->sel_day--; a->cam_follow = 1; return 1; }
        if ((pressed & PSP_CTRL_RIGHT) && a->sel_day < a->today) { a->sel_day++; a->cam_follow = 1; return 1; }
        if (cur & PSP_CTRL_UP)   { a->cam_follow = 0; a->cam += 8; return 1; }
        if (cur & PSP_CTRL_DOWN) { a->cam_follow = 0; a->cam -= 8; if (a->cam < 0) a->cam = 0; return 1; }
        if (pressed & PSP_CTRL_CROSS) { a->cam_follow = 1; return 1; }
        break;
    case VIEW_WEEK:
        if (pressed & PSP_CTRL_LEFT)  { a->week_start -= 7; return 1; }
        if ((pressed & PSP_CTRL_RIGHT) && a->week_start + 7 <= a->today) { a->week_start += 7; return 1; }
        break;
    case VIEW_MONTH:
        if (pressed & PSP_CTRL_LEFT) {
            if (--a->month_m < 1) { a->month_m = 12; a->month_y--; }
            return 1;
        }
        if (pressed & PSP_CTRL_RIGHT) {
            int y, m, d;
            dt_from_day_number(a->today, &y, &m, &d);
            if (a->month_y * 12 + a->month_m < y * 12 + m) {
                if (++a->month_m > 12) { a->month_m = 1; a->month_y++; }
                return 1;
            }
        }
        break;
    case VIEW_YEAR:
        if (pressed & PSP_CTRL_LEFT)  { a->sel_day--; return 1; }
        if (pressed & PSP_CTRL_RIGHT) { if (a->sel_day < a->today) a->sel_day++; return 1; }
        if (pressed & PSP_CTRL_UP)    { a->sel_day -= 7; return 1; }
        if (pressed & PSP_CTRL_DOWN)  { a->sel_day += 7; if (a->sel_day > a->today) a->sel_day = a->today; return 1; }
        break;
    }
    return 0;
}

void views_draw(App *a)
{
    switch (a->view) {
    case VIEW_TODAY: draw_today(a); break;
    case VIEW_WEEK:  draw_week(a);  break;
    case VIEW_MONTH: draw_month(a); break;
    default:         draw_year(a);  break;
    }
}
