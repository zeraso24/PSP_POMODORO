/*
 * Pomodoro City for PSP - version 3
 *
 * A focus timer where every completed focus session builds a floor of the
 * day's building. Saves to the Memory Stick. Built on the official PSPDEV
 * template. Art: "2D Pixel City Pack" by laavis (CC-BY 4.0) and
 * "City Mega Pack" by GrafxKid (CC0).
 *
 * Timer controls:
 *   X        start / pause (every phase waits for you to press X)
 *   SQUARE   skip to the next phase (no floor is built)
 *   TRIANGLE restart the current phase
 *   L / R    change the clock view: Classic, Cinema, Big clock
 *   SELECT   the city (Today / Week / Month / Year)
 *   START    settings
 *   HOME     quit (your progress is saved, the timer comes back paused)
 */
#include <pspuser.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "core.h"
#include "gfx.h"
#include "city.h"
#include "audio.h"
#include "storage.h"
#include "datetime.h"
#include "lang.h"

PSP_MODULE_INFO("Pomodoro", 0, 3, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define POWER_TICK_ALL     0   /* resets every idle timer: backlight stays on */
#define POWER_TICK_SUSPEND 1   /* only blocks auto-sleep: backlight may dim */

#define COL_BG     RGB(18, 18, 24)
#define COL_PANEL  RGB(38, 38, 50)
#define COL_TEXT   RGB(238, 238, 244)
#define COL_DIM    RGB(130, 130, 148)
#define COL_FOCUS  RGB(234, 88, 66)
#define COL_SHORT  RGB(60, 184, 144)
#define COL_LONG   RGB(84, 142, 246)
#define COL_WARN   RGB(240, 90, 90)

/* settings rows: section headers are not selectable */
enum {
    SET_HDR_TIMER, SET_FOCUS, SET_SHORT, SET_LONG, SET_EVERY, SET_AUTO, SET_REMIND,
    SET_HDR_CITY, SET_GOAL, SET_VIEW,
    SET_HDR_SYSTEM, SET_LANG, SET_SOUND, SET_SCREEN, SET_CLOCK, SET_TEST, SET_HOWTO,
    SET_CREDITS, SET_RESET, SET_COUNT
};
#define SET_ROWS_VISIBLE 11

static App g_app;
static volatile int g_exit = 0;
static u32 g_tick_res = 1000000;
static char g_toast_buf[80];
static int g_set_scroll = 0;

/* ------------------------------------------------------------------ */
/* Exit callback (from the official PSPDEV template)                    */
/* ------------------------------------------------------------------ */

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    g_exit = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

/* ------------------------------------------------------------------ */
/* Time, power, messages                                                */
/* ------------------------------------------------------------------ */

static u64 utc_now(void)
{
    u64 t = 0;
    sceRtcGetCurrentTick(&t);
    return t;
}

static u64 local_from_utc(u64 utc)
{
    u64 local = 0;
    if (sceRtcConvertUtcToLocalTime(&utc, &local) < 0)
        return utc;
    return local;
}

static int64_t elapsed_us(u64 from, u64 to)
{
    u64 d, max_d;
    if (to <= from) return 0;
    d = to - from;
    max_d = (u64)86400 * g_tick_res;
    if (d > max_d) d = max_d;
    return (int64_t)(d * 1000000ULL / g_tick_res);
}

/* Local date and time exactly as the PSP shows it, plus the manual fix. */
static void update_clock(App *a, u64 utc)
{
    ScePspDateTime t;
    int64_t day, minute;

    memset(&t, 0, sizeof(t));
    if (sceRtcGetCurrentClockLocalTime(&t) >= 0 && t.year >= 2000 && t.month >= 1 && t.month <= 12) {
        day = dt_day_number(t.year, t.month, t.day);
        minute = t.hour * 60 + t.minute;
    } else {
        u64 secs = local_from_utc(utc) / g_tick_res;
        day = (int64_t)(secs / 86400ULL);
        minute = (int64_t)((secs % 86400ULL) / 60ULL);
    }
    minute += a->p.clock_offset_min;
    while (minute < 0)     { minute += 1440; day--; }
    while (minute >= 1440) { minute -= 1440; day++; }

    a->today = (int32_t)day;
    a->clock_h = (int)(minute / 60);
    a->clock_m = (int)(minute % 60);
    a->minute_of_day = (int)minute;
}

static void update_battery(App *a)
{
    a->battery  = scePowerGetBatteryLifePercent();
    a->charging = scePowerIsBatteryCharging();
}

static void set_fast_cpu(int fast)
{
    static int current = -1;
    if (fast == current) return;
    current = fast;
    if (fast) scePowerSetClockFrequency(333, 333, 166);
    else      scePowerSetClockFrequency(222, 222, 111);
}

static void toast(App *a, const char *msg, int seconds)
{
    strncpy(g_toast_buf, msg, sizeof(g_toast_buf) - 1);
    g_toast_buf[sizeof(g_toast_buf) - 1] = '\0';
    a->toast = g_toast_buf;
    a->toast_t = seconds * 30;
}

static void fmt_duration(char *buf, size_t n, uint32_t secs)
{
    uint32_t h = secs / 3600, m = (secs / 60) % 60;
    if (h > 0) snprintf(buf, n, "%uh %02um", (unsigned)h, (unsigned)m);
    else       snprintf(buf, n, "%um", (unsigned)m);
}

static void do_save(App *a)
{
    a->save_status = (storage_save(&a->p) == 0) ? 1 : -1;
}

static const char *season_name(int tier)
{
    static const StrId ids[5] = { S_SEASON_0, S_SEASON_1, S_SEASON_2, S_SEASON_3, S_SEASON_4 };
    return T(ids[tier < 0 ? 0 : (tier > 4 ? 4 : tier)]);
}

/* A new season palette this month? Announce it once. */
static void check_season(App *a)
{
    int y, m, tier, mark;
    dt_from_day_number(a->today, &y, &m, 0);
    tier = pomo_season_tier(pomo_month_floors(&a->p, y, m));
    mark = (y * 12 + m) * 8 + tier;
    if (tier > 0 && mark > a->p.season_mark && a->p.season_mark / 8 <= y * 12 + m) {
        char buf[80];
        snprintf(buf, sizeof(buf), T(S_T_SEASON_UP), season_name(tier));
        toast(a, buf, 12);
        a->repaint_t = 0;
        audio_play(SND_ROOF);
    }
    if (mark > a->p.season_mark) a->p.season_mark = mark;
}

static void on_new_day(App *a, int32_t old_day)
{
    int y, mo, d;
    if (!a->p.running) {
        a->p.cycle_done = 0;
        if (a->p.phase != PH_FOCUS && pomo_not_started(&a->p)) {
            a->p.phase = PH_FOCUS;
            a->p.remaining_us = pomo_phase_len_us(&a->p, PH_FOCUS);
        }
    }
    if (a->sel_day == old_day) a->sel_day = a->today;
    a->week_start = a->today - dt_weekday(a->today);
    dt_from_day_number(a->today, &y, &mo, &d);
    a->month_y = y;
    a->month_m = mo;
    a->land_t = -1;
    toast(a, a->today > old_day ? T(S_T_NEW_DAY) : T(S_T_CLOCK_CHANGED), 10);
    do_save(a);
}

/* ------------------------------------------------------------------ */
/* Classic timer screen                                                 */
/* ------------------------------------------------------------------ */

static void center_left(int y, int scale, uint32_t color, const char *s)
{
    gfx_text(156 - gfx_text_width(s, scale) / 2, y, scale, color, s);
}

static void draw_classic(App *a, int frame)
{
    Pomo *p = &a->p;
    int goal = (int)p->set.daily_goal;
    int floors = pomo_floors(p, a->today);
    int secs = pomo_remaining_sec(p);
    int waiting = pomo_not_started(p);
    uint32_t accent = p->phase == PH_FOCUS ? COL_FOCUS : (p->phase == PH_SHORT ? COL_SHORT : COL_LONG);
    const char *label;
    char buf[80], dur[24];
    const DayRec *td = pomo_find_day(p, a->today);
    int dd;

    gfx_clear(COL_BG);
    views_timer_window(a, 312, 34, 160, 198);

    gfx_text(16, 12, 2, COL_DIM, "POMODORO");
    dt_from_day_number(a->today, 0, 0, &dd);
    if (a->battery >= 0)
        snprintf(buf, sizeof(buf), "%s %d  %02d:%02d %3d%%%s", lang_weekday(a->today), dd,
                 a->clock_h, a->clock_m, a->battery, a->charging ? "+" : "");
    else
        snprintf(buf, sizeof(buf), "%s %d  %02d:%02d", lang_weekday(a->today), dd, a->clock_h, a->clock_m);
    gfx_text_right(464, 14, 1, COL_DIM, buf);

    if (p->phase == PH_FOCUS) label = T(S_FOCUS);
    else if (waiting) { label = T(S_BREAK_TIME); accent = COL_SHORT; }
    else label = p->phase == PH_SHORT ? T(S_SHORT_BREAK) : T(S_LONG_BREAK);
    center_left(44, 3, accent, label);

    snprintf(buf, sizeof(buf), "%02d:%02d", secs / 60, secs % 60);
    center_left(80, 7, p->running ? COL_TEXT : COL_DIM, buf);

    gfx_rect(16, 148, 280, 6, COL_PANEL);
    gfx_rect(16, 148, (int)(280.0f * pomo_progress(p)), 6, accent);

    if (p->phase == PH_FOCUS) {
        int next = floors + 1;
        if (p->running) {
            snprintf(buf, sizeof(buf), T(S_BUILDING_FLOOR), next);
            center_left(166, 2, COL_TEXT, buf);
            center_left(188, 1, COL_DIM, T(S_CRANE_LOWERS));
        } else if (waiting) {
            center_left(166, 2, COL_TEXT, T(S_PRESS_X));
            if (next > goal) snprintf(buf, sizeof(buf), T(S_BONUS_FLOOR_N), next);
            else snprintf(buf, sizeof(buf), T(S_FLOOR_OF), next, goal);
            center_left(188, 1, COL_DIM, buf);
        } else {
            if ((frame / 30) % 2 == 0) center_left(166, 2, accent, T(S_PAUSED));
            snprintf(buf, sizeof(buf), T(S_FLOOR_WAITING), next);
            center_left(188, 1, COL_DIM, buf);
        }
    } else {
        if (waiting) {
            if ((frame / 30) % 2 == 0 || floors == 0) center_left(166, 2, accent, T(S_PRESS_X));
            if (floors == goal) snprintf(buf, sizeof(buf), "%s", T(S_ROOF_REACHED));
            else if (floors > goal) snprintf(buf, sizeof(buf), "%s", T(S_BONUS_BUILT));
            else snprintf(buf, sizeof(buf), T(S_FLOOR_BUILT), floors, goal - floors);
            center_left(188, 1, COL_TEXT, buf);
        } else {
            center_left(166, 2, COL_DIM, T(S_TAKE_BREAK));
            center_left(188, 1, COL_DIM, T(S_CRANE_WAITS));
        }
    }

    if (a->toast_t > 0 && a->toast)
        gfx_text(16, 212, 1, COL_SHORT, a->toast);
    else if (a->unlock_t > 0 && a->new_unlocks)
        gfx_text(16, 212, 1, RGB(255, 170, 90), T(S_NEW_UNLOCK_HINT));

    fmt_duration(dur, sizeof(dur), td ? td->focus_sec : 0);
    snprintf(buf, sizeof(buf), T(S_TODAY_LINE), floors, dur);
    gfx_text(16, 228, 1, COL_DIM, buf);
    if (a->save_status < 0) gfx_text_right(296, 228, 1, COL_WARN, T(S_SAVE_FAILED));
    else if (a->save_status > 0) gfx_text_right(296, 228, 1, COL_DIM, T(S_SAVED));

    views_help_two_rows(a);
}

/* ------------------------------------------------------------------ */
/* Settings and credits                                                 */
/* ------------------------------------------------------------------ */

static int set_is_header(int i)
{
    return i == SET_HDR_TIMER || i == SET_HDR_CITY || i == SET_HDR_SYSTEM;
}

static const char *set_label(int i)
{
    switch (i) {
    case SET_HDR_TIMER:  return T(S_SEC_TIMER);
    case SET_HDR_CITY:   return T(S_SEC_CITY);
    case SET_HDR_SYSTEM: return T(S_SEC_SYSTEM);
    case SET_FOCUS:   return T(S_SET_FOCUS);
    case SET_SHORT:   return T(S_SET_SHORT);
    case SET_LONG:    return T(S_SET_LONG);
    case SET_EVERY:   return T(S_SET_EVERY);
    case SET_AUTO:    return T(S_SET_AUTO);
    case SET_REMIND:  return T(S_SET_REMIND);
    case SET_GOAL:    return T(S_SET_GOAL);
    case SET_VIEW:    return T(S_SET_VIEW);
    case SET_LANG:    return T(S_SET_LANG);
    case SET_SOUND:   return T(S_SET_SOUND);
    case SET_SCREEN:  return T(S_SET_SCREEN);
    case SET_CLOCK:   return T(S_SET_CLOCK);
    case SET_TEST:    return T(S_SET_TEST);
    case SET_HOWTO:   return T(S_SET_HOWTO);
    case SET_CREDITS: return T(S_SET_CREDITS);
    default:          return T(S_SET_RESET);
    }
}

static void settings_value(App *a, int item, char *buf, size_t n)
{
    static const StrId vol[4] = { S_VOL_OFF, S_VOL_LOW, S_VOL_MED, S_VOL_HIGH };
    static const StrId views[3] = { S_VIEW_CLASSIC, S_VIEW_CINEMA, S_VIEW_BIG };
    Settings *s = &a->p.set;
    char v[40];
    v[0] = '\0';
    switch (item) {
    case SET_FOCUS:  snprintf(v, sizeof(v), T(S_V_MIN), (unsigned)s->focus_min); break;
    case SET_SHORT:  snprintf(v, sizeof(v), T(S_V_MIN), (unsigned)s->short_min); break;
    case SET_LONG:   snprintf(v, sizeof(v), T(S_V_MIN), (unsigned)s->long_min); break;
    case SET_EVERY:  snprintf(v, sizeof(v), T(S_V_SESSIONS), (unsigned)s->long_every); break;
    case SET_AUTO:   snprintf(v, sizeof(v), "%s", s->auto_start ? T(S_V_ON) : T(S_V_OFF)); break;
    case SET_REMIND:
        if (a->p.reminder_min) snprintf(v, sizeof(v), T(S_V_MIN), (unsigned)a->p.reminder_min);
        else snprintf(v, sizeof(v), "%s", T(S_V_OFF));
        break;
    case SET_GOAL:   snprintf(v, sizeof(v), T(S_V_FLOORS), (unsigned)s->daily_goal); break;
    case SET_VIEW:   snprintf(v, sizeof(v), "%s", T(views[a->p.timer_view % 3])); break;
    case SET_LANG:   snprintf(v, sizeof(v), "%s", lang_language_name(a->p.language)); break;
    case SET_SOUND:  snprintf(v, sizeof(v), "%s", T(vol[s->volume & 3])); break;
    case SET_SCREEN: snprintf(v, sizeof(v), "%s", s->keep_screen ? T(S_V_ON) : T(S_V_OFF)); break;
    case SET_CLOCK: {
        int off = a->p.clock_offset_min, ao = off < 0 ? -off : off;
        snprintf(v, sizeof(v), "%s%d:%02d %02d:%02d", off < 0 ? "-" : "+", ao / 60, ao % 60,
                 a->clock_h, a->clock_m);
        break;
    }
    case SET_TEST: case SET_HOWTO: case SET_CREDITS:
        snprintf(buf, n, "%s", T(S_V_PRESS_X));
        return;
    case SET_RESET:
        snprintf(buf, n, "%s", a->reset_armed ? T(S_V_PRESS_AGAIN) : T(S_V_PRESS_X));
        return;
    default:
        buf[0] = '\0';
        return;
    }
    snprintf(buf, n, "< %s >", v);
}

static void draw_settings(App *a)
{
    char buf[48];
    int r, x;

    gfx_clear(COL_BG);
    gfx_text(16, 8, 2, COL_DIM, T(S_SETTINGS));

    if (a->sel < g_set_scroll + 1) g_set_scroll = a->sel - 1;
    if (a->sel >= g_set_scroll + SET_ROWS_VISIBLE) g_set_scroll = a->sel - SET_ROWS_VISIBLE + 1;
    if (g_set_scroll < 0) g_set_scroll = 0;
    if (g_set_scroll > SET_COUNT - SET_ROWS_VISIBLE) g_set_scroll = SET_COUNT - SET_ROWS_VISIBLE;

    for (r = 0; r < SET_ROWS_VISIBLE; r++) {
        int i = g_set_scroll + r;
        int y = 30 + r * 19;
        if (i >= SET_COUNT) break;
        if (set_is_header(i)) {
            gfx_text(16, y + 5, 1, COL_FOCUS, set_label(i));
            gfx_rect(16 + gfx_text_width(set_label(i), 1) + 8, y + 8, 440 - gfx_text_width(set_label(i), 1), 1, COL_PANEL);
            continue;
        }
        {
            int selected = (i == a->sel);
            uint32_t vcol = COL_TEXT;
            if (selected) {
                gfx_rect(12, y - 1, 456, 18, COL_PANEL);
                gfx_rect(12, y - 1, 3, 18, COL_FOCUS);
            }
            gfx_text(24, y, 2, selected ? COL_TEXT : COL_DIM, set_label(i));
            settings_value(a, i, buf, sizeof(buf));
            if (i == SET_RESET && a->reset_armed) vcol = COL_WARN;
            else if (!selected) vcol = COL_DIM;
            gfx_text_right(460, y, 2, vcol, buf);
        }
    }
    /* scroll hints */
    if (g_set_scroll > 0) gfx_text_right(470, 10, 1, COL_DIM, "^");
    if (g_set_scroll < SET_COUNT - SET_ROWS_VISIBLE) gfx_text_right(470, 238, 1, COL_DIM, "v");

    gfx_rect(0, 248, SCREEN_W, 24, COL_BG);
    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    if (a->reset_armed) {
        gfx_text(12, 256, 1, COL_WARN, T(S_RESET_WARNING));
        return;
    }
    x = 12;
    x = gfx_text(x, 256, 1, COL_TEXT, "^v");
    x = gfx_text(x + 6, 256, 1, COL_DIM, T(S_H_SELECT)) + 14;
    x = gfx_text(x, 256, 1, COL_TEXT, "<>");
    x = gfx_text(x + 6, 256, 1, COL_DIM, T(S_H_CHANGE)) + 14;
    x = gfx_icon(x, 256, 1, ICON_CROSS);
    x = gfx_text(x + 4, 256, 1, COL_DIM, T(S_H_ACTION)) + 14;
    x = gfx_icon(x, 256, 1, ICON_CIRCLE);
    gfx_text(x + 4, 256, 1, COL_DIM, T(S_H_BACK));
}

static void draw_credits(void)
{
    gfx_clear(COL_BG);
    gfx_text(16, 12, 2, COL_DIM, T(S_CREDITS));
    gfx_text(24, 42, 2, COL_TEXT, "Pomodoro City");
    gfx_text(24, 62, 1, COL_DIM, T(S_CR_VERSION));
    gfx_text(24, 86, 1, COL_TEXT, T(S_CR_BUILDINGS));
    gfx_text(24, 100, 2, RGB(255, 170, 90), "2D Pixel City Pack");
    gfx_text(24, 120, 1, COL_TEXT, "laavis - CC-BY 4.0");
    gfx_text(24, 132, 1, COL_DIM, "opengameart.org/content/2d-pixel-city-pack");
    gfx_text(24, 154, 1, COL_TEXT, T(S_CR_RESIDENTS));
    gfx_text(24, 168, 2, RGB(120, 200, 255), "City Mega Pack");
    gfx_text(24, 188, 1, COL_TEXT, "GrafxKid - CC0");
    gfx_text(24, 200, 1, COL_DIM, "opengameart.org/content/city-mega-pack");
    gfx_text(24, 222, 1, COL_TEXT, T(S_CR_TOOLCHAIN));
    gfx_text(24, 234, 1, COL_DIM, "pspdev.github.io");
    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    gfx_icon(12, 256, 1, ICON_CIRCLE);
    gfx_text(24, 256, 1, COL_DIM, T(S_H_BACK));
}

/* ------------------------------------------------------------------ */
/* Input                                                                */
/* ------------------------------------------------------------------ */

static int repeat_btn(u32 cur, u32 pressed, u32 btn, int *hold)
{
    if (pressed & btn) { *hold = 0; return 1; }
    if (cur & btn) {
        (*hold)++;
        if (*hold > 24 && ((*hold - 24) % 5) == 0) return 1;
    } else {
        *hold = 0;
    }
    return 0;
}

static void adjust_setting(App *a, int item, int dir)
{
    Settings *s = &a->p.set;
    switch (item) {
    case SET_FOCUS:  s->focus_min  = (uint32_t)((int)s->focus_min + dir); break;
    case SET_SHORT:  s->short_min  = (uint32_t)((int)s->short_min + dir); break;
    case SET_LONG:   s->long_min   = (uint32_t)((int)s->long_min + dir); break;
    case SET_EVERY:  s->long_every = (uint32_t)((int)s->long_every + dir); break;
    case SET_GOAL:   s->daily_goal = (uint32_t)((int)s->daily_goal + dir); break;
    case SET_AUTO:   s->auto_start = !s->auto_start; break;
    case SET_SCREEN: s->keep_screen = !s->keep_screen; break;
    case SET_REMIND: {
        static const int opts[4] = { 0, 3, 5, 10 };
        int i = 0;
        while (i < 3 && opts[i] != a->p.reminder_min) i++;
        i = (i + (dir > 0 ? 1 : 3)) % 4;
        a->p.reminder_min = opts[i];
        break;
    }
    case SET_VIEW:
        a->p.timer_view = (a->p.timer_view + (dir > 0 ? 1 : TV_COUNT - 1)) % TV_COUNT;
        break;
    case SET_LANG:
        a->p.language = a->p.language == LANG_ES ? LANG_EN : LANG_ES;
        lang_set(a->p.language);
        break;
    case SET_CLOCK: {
        int off = a->p.clock_offset_min + dir * 30;
        if (off < -720) off = -720;
        if (off > 720) off = 720;
        a->p.clock_offset_min = off;
        break;
    }
    case SET_SOUND: {
        int v = (int)s->volume + dir;
        if (v < VOL_OFF) v = VOL_OFF;
        if (v > VOL_HIGH) v = VOL_HIGH;
        s->volume = (uint32_t)v;
        audio_set_volume(v);
        audio_play(SND_CLICK);
        break;
    }
    default: break;
    }
    if ((int32_t)s->focus_min < 1)  s->focus_min = 1;
    if ((int32_t)s->short_min < 1)  s->short_min = 1;
    if ((int32_t)s->long_min < 1)   s->long_min = 1;
    if ((int32_t)s->long_every < 2) s->long_every = 2;
    if ((int32_t)s->daily_goal < 4) s->daily_goal = 4;
    pomo_clamp_settings(s);
}

static void leave_settings(App *a)
{
    pomo_settings_changed(&a->p, &a->set_before);
    audio_set_volume((int)a->p.set.volume);
    a->reset_armed = 0;
    a->screen = SCR_TIMER;
    do_save(a);
}

static void move_sel(App *a, int dir)
{
    do {
        a->sel = (a->sel + dir + SET_COUNT) % SET_COUNT;
    } while (set_is_header(a->sel));
}

static int handle_input(App *a, u32 cur, u32 pressed)
{
    static int hold_up, hold_down, hold_left, hold_right;
    Pomo *p = &a->p;

    switch (a->screen) {
    case SCR_TIMER:
        if (pressed & PSP_CTRL_CROSS) {
            pomo_toggle(p);
            audio_play(SND_CLICK);
            a->wait_ticks = 0;
            a->remind_count = 0;
            if (!p->running) do_save(a);
            return 1;
        }
        if (pressed & PSP_CTRL_SQUARE) {
            pomo_skip(p);
            audio_play(SND_CLICK);
            do_save(a);
            return 1;
        }
        if (pressed & PSP_CTRL_TRIANGLE) {
            pomo_reset_phase(p);
            audio_play(SND_CLICK);
            do_save(a);
            return 1;
        }
        if (pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
            int d = (pressed & PSP_CTRL_RTRIGGER) ? 1 : TV_COUNT - 1;
            p->timer_view = (p->timer_view + d) % TV_COUNT;
            audio_play(SND_CLICK);
            return 1;
        }
        if (pressed & PSP_CTRL_SELECT) {
            a->screen = SCR_CITY;
            views_open(a, VIEW_TODAY);
            return 1;
        }
        if (pressed & PSP_CTRL_START) {
            a->set_before = p->set;
            a->sel = SET_FOCUS;
            g_set_scroll = 0;
            a->reset_armed = 0;
            a->screen = SCR_SETTINGS;
            return 1;
        }
        return 0;

    case SCR_CITY:
        return views_input(a, cur, pressed);

    case SCR_CREDITS:
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_START)) {
            a->screen = SCR_SETTINGS;
            return 1;
        }
        return 0;

    case SCR_TUTORIAL:
        if (pressed & PSP_CTRL_CROSS) {
            if (++a->tut_page >= 3) {
                p->tutorial_done = 1;
                a->screen = SCR_TIMER;
                do_save(a);
            }
            return 1;
        }
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
            p->tutorial_done = 1;
            a->screen = SCR_TIMER;
            do_save(a);
            return 1;
        }
        return 0;

    default:
        break;
    }

    /* settings */
    if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
        leave_settings(a);
        return 1;
    }
    if (repeat_btn(cur, pressed, PSP_CTRL_UP, &hold_up))     { move_sel(a, -1); a->reset_armed = 0; return 1; }
    if (repeat_btn(cur, pressed, PSP_CTRL_DOWN, &hold_down)) { move_sel(a, +1); a->reset_armed = 0; return 1; }
    if (repeat_btn(cur, pressed, PSP_CTRL_LEFT, &hold_left))   { adjust_setting(a, a->sel, -1); return 1; }
    if (repeat_btn(cur, pressed, PSP_CTRL_RIGHT, &hold_right)) { adjust_setting(a, a->sel, +1); return 1; }
    if (pressed & PSP_CTRL_CROSS) {
        switch (a->sel) {
        case SET_TEST:
            audio_set_volume((int)p->set.volume);
            audio_play(SND_FOCUS_DONE);
            break;
        case SET_CREDITS:
            a->screen = SCR_CREDITS;
            break;
        case SET_HOWTO:
            pomo_settings_changed(p, &a->set_before);
            a->set_before = p->set;
            a->tut_page = 0;
            a->screen = SCR_TUTORIAL;
            break;
        case SET_RESET:
            if (a->reset_armed) {
                pomo_reset_stats(p);
                a->reset_armed = 0;
                do_save(a);
                audio_play(SND_CLICK);
            } else {
                a->reset_armed = 1;
            }
            break;
        case SET_AUTO: case SET_SCREEN: case SET_LANG: case SET_VIEW: case SET_REMIND:
            adjust_setting(a, a->sel, +1);
            break;
        default:
            break;
        }
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    App *a = &g_app;
    SceCtrlData pad;
    u32 prev_buttons = 0;
    u64 last_tick, last_save, last_power, last_battery;
    int frame = 0;
    int dirty = 1;
    int32_t prev_today;
    u32 res;

    setup_callbacks();

    res = sceRtcGetTickResolution();
    if (res > 0) g_tick_res = res;

    memset(a, 0, sizeof(*a));
    a->land_t = -1;
    a->celebrate = -1;
    a->repaint_t = -1;

    storage_init(argc > 0 ? argv[0] : NULL);
    pomo_defaults(&a->p);
    a->load_status = storage_load(&a->p);
    lang_set(a->p.language);

    gfx_init();
    city_init();
    audio_init();
    audio_set_volume((int)a->p.set.volume);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    last_tick = utc_now();
    last_save = last_tick;
    last_power = last_tick;
    last_battery = 0;
    update_clock(a, last_tick);
    prev_today = a->today;
    update_battery(a);
    pomo_check_unlocks(&a->p, a->today);
    {   /* seasons already reached before this version: no fanfare */
        int y, m;
        dt_from_day_number(a->today, &y, &m, 0);
        if (a->p.season_mark == 0)
            a->p.season_mark = (y * 12 + m) * 8 + pomo_season_tier(pomo_month_floors(&a->p, y, m));
    }
    if (a->load_status == 2) {
        toast(a, T(S_T_SAVE_CONVERTED), 10);
        do_save(a);
    }
    if (!a->p.tutorial_done) {
        a->screen = SCR_TUTORIAL;
        a->tut_page = 0;
    }

    while (!g_exit) {
        u64 now = utc_now();
        int64_t dt = elapsed_us(last_tick, now);
        int ev, render;
        u32 pressed;

        last_tick = now;
        update_clock(a, now);
        if (a->today != prev_today) {
            on_new_day(a, prev_today);
            prev_today = a->today;
            dirty = 1;
        }

        /* animation clock: 30 ticks per second */
        if (frame % 2 == 0) {
            a->anim++;
            a->idle_t++;
            if (a->land_t >= 0 && ++a->land_t > 60) a->land_t = -1;
            if (a->repaint_t >= 0 && ++a->repaint_t > 60) a->repaint_t = -1;
            if (a->celebrate >= 0 && ++a->celebrate > 200) {
                a->celebrate = -1;
                a->cam_follow = 0;
                a->cam = 0;
            }
            if (a->unlock_t > 0 && a->celebrate < 0) a->unlock_t--;
            if (a->toast_t > 0) a->toast_t--;

            /* gentle reminder when a finished phase keeps waiting for X */
            if (!a->p.running && pomo_not_started(&a->p) && a->p.reminder_min > 0 && a->remind_count < 5 &&
                (a->p.phase != PH_FOCUS || pomo_floors(&a->p, a->today) > 0)) {
                if (++a->wait_ticks >= a->p.reminder_min * 60 * 30) {
                    a->wait_ticks = 0;
                    a->remind_count++;
                    audio_play(SND_BREAK_DONE);
                    toast(a, T(S_T_REMIND), 8);
                    dirty = 1;
                }
            }
        }

        ev = pomo_tick(&a->p, dt, a->today, a->clock_h);
        if (ev & EV_FOCUS_DONE) {
            uint32_t added = pomo_check_unlocks(&a->p, a->today);
            if (added) { a->new_unlocks = added; a->unlock_t = 30 * 12; }
            a->land_t = 0;
            if (ev & EV_ROOF) {
                audio_play(SND_ROOF);
                views_start_celebration(a);
            } else {
                audio_play((ev & EV_LONG_NEXT) ? SND_LONG_BREAK : SND_FOCUS_DONE);
            }
            check_season(a);
        } else if (ev & EV_BREAK_DONE) {
            audio_play(SND_BREAK_DONE);
        }
        if (ev & (EV_FOCUS_DONE | EV_BREAK_DONE)) {
            a->wait_ticks = 0;
            a->remind_count = 0;
            do_save(a);
            last_save = now;
            dirty = 1;
        }

        if (a->p.running && elapsed_us(last_save, now) >= 120LL * US_PER_SEC) {
            do_save(a);
            last_save = now;
        }
        if (a->p.running && elapsed_us(last_power, now) >= 5LL * US_PER_SEC) {
            scePowerTick(a->p.set.keep_screen ? POWER_TICK_ALL : POWER_TICK_SUSPEND);
            last_power = now;
        }
        if (last_battery == 0 || elapsed_us(last_battery, now) >= 10LL * US_PER_SEC) {
            update_battery(a);
            last_battery = now;
        }

        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev_buttons;
        if (pressed) a->idle_t = 0;
        if (handle_input(a, pad.Buttons, pressed))
            dirty = 1;
        prev_buttons = pad.Buttons;

        /* frame rate per screen: city 30, cinema 20, classic 15, big clock 1 */
        {
            int cinema = a->screen == SCR_TIMER && a->p.timer_view == TV_CINEMA;
            set_fast_cpu(a->screen == SCR_CITY || cinema);
            if (a->screen == SCR_CITY || a->screen == SCR_TUTORIAL) render = dirty || (frame % 2) == 0;
            else if (cinema)                                      render = dirty || (frame % 3) == 0;
            else if (a->screen == SCR_TIMER && a->p.timer_view == TV_BIG)
                render = dirty || (frame % 60) == 0 || a->toast_t > 0;
            else if (a->screen == SCR_TIMER)                      render = dirty || (frame % 4) == 0;
            else                                                  render = dirty || (frame % 30) == 0;
        }

        if (render) {
            switch (a->screen) {
            case SCR_TIMER:
                if (a->p.timer_view == TV_CINEMA)   views_cinema(a);
                else if (a->p.timer_view == TV_BIG) views_bigclock(a);
                else                                draw_classic(a, frame);
                break;
            case SCR_CITY:     views_draw(a);      break;
            case SCR_SETTINGS: draw_settings(a);   break;
            case SCR_TUTORIAL: views_tutorial(a);  break;
            default:           draw_credits();     break;
            }
            gfx_present();
            dirty = 0;
        } else {
            gfx_wait_vblank();
        }
        frame++;
    }

    if (a->screen == SCR_SETTINGS || a->screen == SCR_CREDITS)
        pomo_settings_changed(&a->p, &a->set_before);
    storage_save(&a->p);
    audio_shutdown();
    set_fast_cpu(0);
    sceKernelExitGame();
    return 0;
}
