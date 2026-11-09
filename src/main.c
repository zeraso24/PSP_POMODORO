/*
 * Pomodoro for PSP
 *
 * A focus timer that tracks your sessions and saves them to the Memory Stick.
 * Built on the official PSPDEV template (https://pspdev.github.io).
 *
 * Controls (timer screen):
 *   X        start / pause
 *   SQUARE   skip to the next phase
 *   TRIANGLE restart the current phase
 *   SELECT   statistics
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

#include "core.h"
#include "gfx.h"
#include "audio.h"
#include "storage.h"
#include "datetime.h"

PSP_MODULE_INFO("Pomodoro", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

/* scePowerTick() types */
#define POWER_TICK_ALL     0   /* resets every idle timer: backlight stays on */
#define POWER_TICK_SUSPEND 1   /* only blocks auto-sleep: backlight may dim */

/* Colors */
#define COL_BG     RGB(18, 18, 24)
#define COL_PANEL  RGB(38, 38, 50)
#define COL_TEXT   RGB(238, 238, 244)
#define COL_DIM    RGB(130, 130, 148)
#define COL_FOCUS  RGB(234, 88, 66)
#define COL_SHORT  RGB(60, 184, 144)
#define COL_LONG   RGB(84, 142, 246)
#define COL_WARN   RGB(240, 90, 90)

enum { SCR_TIMER = 0, SCR_STATS, SCR_SETTINGS };

enum {
    SET_FOCUS = 0, SET_SHORT, SET_LONG, SET_EVERY, SET_AUTO,
    SET_SOUND, SET_SCREEN, SET_TEST, SET_RESET, SET_COUNT
};

typedef struct {
    Pomo     p;
    int      screen;
    int      sel;             /* selected settings row */
    int      reset_armed;     /* waiting for the second X on "Reset stats" */
    Settings set_before;      /* settings when the settings screen opened */
    int      save_status;     /* 1 ok, -1 failed, 0 not saved yet */
    int      load_status;     /* storage_load() result */
    int32_t  today;           /* local day number */
    int      clock_h, clock_m;
    int      battery;         /* percent, <0 if unknown */
    int      charging;
} App;

static App g_app;
static volatile int g_exit = 0;
static u32 g_tick_res = 1000000;

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
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
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
/* Time                                                                 */
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

/* Elapsed real time in microseconds; caps absurd jumps at one day. */
static int64_t elapsed_us(u64 from, u64 to)
{
    u64 d, max_d;
    if (to <= from) return 0;
    d = to - from;
    max_d = (u64)86400 * g_tick_res;
    if (d > max_d) d = max_d;
    return (int64_t)(d * 1000000ULL / g_tick_res);
}

static void update_clock(App *a, u64 utc)
{
    u64 local = local_from_utc(utc);
    u64 secs = local / g_tick_res;
    u64 sec_of_day = secs % 86400ULL;
    a->today   = (int32_t)(secs / 86400ULL);
    a->clock_h = (int)(sec_of_day / 3600ULL);
    a->clock_m = (int)((sec_of_day / 60ULL) % 60ULL);
}

static void update_battery(App *a)
{
    a->battery  = scePowerGetBatteryLifePercent();
    a->charging = scePowerIsBatteryCharging();
}

/* ------------------------------------------------------------------ */
/* Formatting                                                           */
/* ------------------------------------------------------------------ */

static void fmt_duration(char *buf, size_t n, uint32_t secs)
{
    uint32_t h = secs / 3600;
    uint32_t m = (secs / 60) % 60;
    if (h > 0) snprintf(buf, n, "%uh %02um", (unsigned)h, (unsigned)m);
    else       snprintf(buf, n, "%um", (unsigned)m);
}

static uint32_t phase_color(uint32_t phase)
{
    if (phase == PH_SHORT) return COL_SHORT;
    if (phase == PH_LONG)  return COL_LONG;
    return COL_FOCUS;
}

static const char *phase_name(uint32_t phase)
{
    if (phase == PH_SHORT) return "SHORT BREAK";
    if (phase == PH_LONG)  return "LONG BREAK";
    return "FOCUS";
}

static void do_save(App *a)
{
    a->save_status = (storage_save(&a->p) == 0) ? 1 : -1;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                              */
/* ------------------------------------------------------------------ */

static int help_icon(int x, int y, int icon, const char *label)
{
    x = gfx_icon(x, y, 1, icon);
    x = gfx_text(x + 4, y, 1, COL_DIM, label);
    return x + 14;
}

static int help_text(int x, int y, const char *key, const char *label)
{
    x = gfx_text(x, y, 1, COL_TEXT, key);
    x = gfx_text(x + 6, y, 1, COL_DIM, label);
    return x + 14;
}

static void draw_header(App *a, const char *title)
{
    char buf[32];
    gfx_text(16, 12, 2, COL_DIM, title);

    if (a->battery >= 0)
        snprintf(buf, sizeof(buf), "%02d:%02d %3d%%%s", a->clock_h, a->clock_m,
                 a->battery, a->charging ? "+" : "");
    else
        snprintf(buf, sizeof(buf), "%02d:%02d", a->clock_h, a->clock_m);
    gfx_text_right(464, 12, 2, COL_DIM, buf);
}

static void draw_timer(App *a, int frame)
{
    Pomo *p = &a->p;
    uint32_t accent = phase_color(p->phase);
    int secs = pomo_remaining_sec(p);
    int not_started = !p->running && p->remaining_us == pomo_phase_len_us(p, p->phase);
    char buf[64], dur[24];
    int i, n, filled, dot, gap, total, x;
    const DayRec *td;

    gfx_clear(COL_BG);
    draw_header(a, "POMODORO");

    /* phase name */
    gfx_text_center(46, 3, accent, phase_name(p->phase));

    /* big countdown */
    snprintf(buf, sizeof(buf), "%02d:%02d", secs / 60, secs % 60);
    gfx_text_center(80, 10, p->running ? COL_TEXT : COL_DIM, buf);

    /* progress bar */
    gfx_rect(40, 172, 400, 6, COL_PANEL);
    gfx_rect(40, 172, (int)(400.0f * pomo_progress(p)), 6, accent);

    /* cycle dots: focus sessions done before the long break */
    n = (int)p->set.long_every;
    filled = (p->phase == PH_LONG) ? n : (int)p->cycle_done;
    if (filled > n) filled = n;
    dot = 10; gap = 10;
    total = n * dot + (n - 1) * gap;
    x = (SCREEN_W - total) / 2;
    for (i = 0; i < n; i++) {
        if (i < filled) gfx_rect(x, 188, dot, dot, COL_FOCUS);
        else            gfx_frame(x, 188, dot, dot, COL_DIM);
        x += dot + gap;
    }

    /* status line */
    if (!p->running) {
        if (not_started)
            gfx_text_center(210, 2, COL_TEXT, "Press X to start");
        else if ((frame / 30) % 2 == 0)
            gfx_text_center(210, 2, accent, "PAUSED");
    } else if (p->phase == PH_FOCUS) {
        snprintf(buf, sizeof(buf), "Session %u of %u",
                 (unsigned)(p->cycle_done + 1), (unsigned)p->set.long_every);
        gfx_text_center(210, 2, COL_DIM, buf);
    } else {
        gfx_text_center(210, 2, COL_DIM, "Take a break");
    }

    /* today summary + save state */
    td = pomo_find_day(p, a->today);
    fmt_duration(dur, sizeof(dur), td ? td->focus_sec : 0);
    snprintf(buf, sizeof(buf), "Today: %u sessions, %s focused",
             (unsigned)(td ? td->sessions : 0), dur);
    gfx_text(16, 234, 1, COL_DIM, buf);
    if (a->save_status < 0)
        gfx_text_right(464, 234, 1, COL_WARN, "SAVE FAILED");
    else if (a->save_status > 0)
        gfx_text_right(464, 234, 1, COL_DIM, "Saved");

    /* controls */
    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    x = 12;
    x = help_icon(x, 256, ICON_CROSS, p->running ? "Pause" : "Start");
    x = help_icon(x, 256, ICON_SQUARE, "Skip");
    x = help_icon(x, 256, ICON_TRIANGLE, "Restart");
    x = help_text(x, 256, "SELECT", "Stats");
    help_text(x, 256, "START", "Settings");
}

static void draw_card(int x, int y, const char *label, const char *value, const char *sub)
{
    gfx_rect(x, y, 108, 62, COL_PANEL);
    gfx_text(x + 8, y + 8, 1, COL_DIM, label);
    gfx_text(x + 8, y + 22, 3, COL_TEXT, value);
    gfx_text(x + 8, y + 50, 1, COL_DIM, sub);
}

static void draw_stats(App *a)
{
    Pomo *p = &a->p;
    char v[24], s[24];
    uint32_t wk_s, wk_f, streak, maxm, mins[7];
    const DayRec *td = pomo_find_day(p, a->today);
    int i, y, m, d, slot, base_y, bar_w;

    gfx_clear(COL_BG);
    gfx_text(16, 12, 2, COL_DIM, "STATISTICS");
    dt_from_day_number(a->today, &y, &m, &d);
    snprintf(v, sizeof(v), "%04d-%02d-%02d", y, m, d);
    gfx_text_right(464, 12, 2, COL_DIM, v);

    /* cards */
    snprintf(v, sizeof(v), "%u", (unsigned)(td ? td->sessions : 0));
    fmt_duration(s, sizeof(s), td ? td->focus_sec : 0);
    draw_card(12, 38, "TODAY", v, s);

    pomo_range_totals(p, a->today - 6, a->today, &wk_s, &wk_f);
    snprintf(v, sizeof(v), "%u", (unsigned)wk_s);
    fmt_duration(s, sizeof(s), wk_f);
    draw_card(128, 38, "LAST 7 DAYS", v, s);

    snprintf(v, sizeof(v), "%u", (unsigned)p->total_sessions);
    fmt_duration(s, sizeof(s), p->total_focus_sec);
    draw_card(244, 38, "ALL TIME", v, s);

    streak = pomo_streak(p, a->today);
    snprintf(v, sizeof(v), "%u", (unsigned)streak);
    draw_card(360, 38, "STREAK", v, streak == 1 ? "day" : "days");

    /* last 7 days chart */
    gfx_text(16, 110, 1, COL_DIM, "FOCUS MINUTES, LAST 7 DAYS");
    maxm = 1;
    for (i = 0; i < 7; i++) {
        const DayRec *r = pomo_find_day(p, a->today - 6 + i);
        mins[i] = r ? r->focus_sec / 60 : 0;
        if (mins[i] > maxm) maxm = mins[i];
    }
    base_y = 222;
    slot = 432 / 7;
    bar_w = 34;
    gfx_rect(24, base_y, 432, 1, COL_PANEL);
    for (i = 0; i < 7; i++) {
        int32_t day = a->today - 6 + i;
        int cx = 24 + i * slot + slot / 2;
        int h = (int)(78.0f * (float)mins[i] / (float)maxm);
        uint32_t col = (i == 6) ? COL_FOCUS : RGB(150, 72, 60);
        const char *wd = dt_weekday_name(day);

        if (mins[i] > 0 && h < 2) h = 2;
        gfx_rect(cx - bar_w / 2, base_y - h, bar_w, h, col);
        if (mins[i] > 0) {
            snprintf(v, sizeof(v), "%um", (unsigned)mins[i]);
            gfx_text(cx - gfx_text_width(v, 1) / 2, base_y - h - 10, 1, COL_TEXT, v);
        }
        gfx_text(cx - gfx_text_width(wd, 1) / 2, base_y + 6,
                 1, (i == 6) ? COL_TEXT : COL_DIM, wd);
    }

    if (a->load_status < 0)
        gfx_text(16, 238, 1, COL_WARN, "Save file was damaged and could not be read.");

    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    help_icon(12, 256, ICON_CIRCLE, "Back");
}

static const char *VOLUME_NAMES[4] = { "Off", "Low", "Medium", "High" };

static void settings_value(App *a, int item, char *buf, size_t n)
{
    Settings *s = &a->p.set;
    switch (item) {
    case SET_FOCUS:  snprintf(buf, n, "< %u min >", (unsigned)s->focus_min); break;
    case SET_SHORT:  snprintf(buf, n, "< %u min >", (unsigned)s->short_min); break;
    case SET_LONG:   snprintf(buf, n, "< %u min >", (unsigned)s->long_min); break;
    case SET_EVERY:  snprintf(buf, n, "< %u sessions >", (unsigned)s->long_every); break;
    case SET_AUTO:   snprintf(buf, n, "< %s >", s->auto_start ? "On" : "Off"); break;
    case SET_SOUND:  snprintf(buf, n, "< %s >", VOLUME_NAMES[s->volume & 3]); break;
    case SET_SCREEN: snprintf(buf, n, "< %s >", s->keep_screen ? "On" : "Off"); break;
    case SET_TEST:   snprintf(buf, n, "Press X"); break;
    case SET_RESET:  snprintf(buf, n, a->reset_armed ? "Press X again" : "Press X"); break;
    default:         buf[0] = '\0'; break;
    }
}

static void draw_settings(App *a)
{
    static const char *labels[SET_COUNT] = {
        "Focus", "Short break", "Long break", "Long after", "Auto-start",
        "Sound", "Keep screen", "Test sound", "Reset stats"
    };
    char buf[32];
    int i, x;

    gfx_clear(COL_BG);
    gfx_text(16, 12, 2, COL_DIM, "SETTINGS");

    for (i = 0; i < SET_COUNT; i++) {
        int y = 40 + i * 22;
        int selected = (i == a->sel);
        uint32_t vcol = COL_TEXT;

        if (selected) {
            gfx_rect(12, y - 3, 456, 21, COL_PANEL);
            gfx_rect(12, y - 3, 3, 21, COL_FOCUS);
        }
        gfx_text(24, y, 2, selected ? COL_TEXT : COL_DIM, labels[i]);
        settings_value(a, i, buf, sizeof(buf));
        if (i == SET_RESET && a->reset_armed) vcol = COL_WARN;
        else if (!selected) vcol = COL_DIM;
        gfx_text_right(460, y, 2, vcol, buf);
    }

    if (a->reset_armed)
        gfx_text(16, 238, 1, COL_WARN, "This erases all your history. Press X again to confirm.");

    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    x = 12;
    x = help_text(x, 256, "UP/DOWN", "Select");
    x = help_text(x, 256, "LEFT/RIGHT", "Change");
    x = help_icon(x, 256, ICON_CROSS, "Action");
    help_icon(x, 256, ICON_CIRCLE, "Back");
}

/* ------------------------------------------------------------------ */
/* Input                                                                */
/* ------------------------------------------------------------------ */

/* Fires on press, then repeats while held (for adjusting values). */
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
    case SET_AUTO:   s->auto_start = !s->auto_start; break;
    case SET_SCREEN: s->keep_screen = !s->keep_screen; break;
    case SET_SOUND: {
        int v = (int)s->volume + dir;
        if (v < VOL_OFF) v = VOL_OFF;
        if (v > VOL_HIGH) v = VOL_HIGH;
        s->volume = (uint32_t)v;
        audio_set_volume(v);
        audio_play(SND_CLICK);   /* preview the new level */
        break;
    }
    default: break;
    }
    /* values below 1 would wrap around as unsigned; clamp handles both ends */
    if ((int32_t)s->focus_min < 1)  s->focus_min = 1;
    if ((int32_t)s->short_min < 1)  s->short_min = 1;
    if ((int32_t)s->long_min < 1)   s->long_min = 1;
    if ((int32_t)s->long_every < 2) s->long_every = 2;
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

/* Returns 1 if something changed on screen. */
static int handle_input(App *a, u32 cur, u32 pressed)
{
    static int hold_up, hold_down, hold_left, hold_right;
    Pomo *p = &a->p;

    if (a->screen == SCR_TIMER) {
        if (pressed & PSP_CTRL_CROSS) {
            pomo_toggle(p);
            audio_play(SND_CLICK);
            if (!p->running) do_save(a);   /* paused: store progress */
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
        if (pressed & PSP_CTRL_SELECT) {
            a->screen = SCR_STATS;
            return 1;
        }
        if (pressed & PSP_CTRL_START) {
            a->set_before = p->set;
            a->sel = 0;
            a->reset_armed = 0;
            a->screen = SCR_SETTINGS;
            return 1;
        }
        return 0;
    }

    if (a->screen == SCR_STATS) {
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_CROSS)) {
            a->screen = SCR_TIMER;
            return 1;
        }
        return 0;
    }

    /* settings */
    if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
        leave_settings(a);
        return 1;
    }
    if (repeat_btn(cur, pressed, PSP_CTRL_UP, &hold_up)) {
        a->sel = (a->sel + SET_COUNT - 1) % SET_COUNT;
        a->reset_armed = 0;
        return 1;
    }
    if (repeat_btn(cur, pressed, PSP_CTRL_DOWN, &hold_down)) {
        a->sel = (a->sel + 1) % SET_COUNT;
        a->reset_armed = 0;
        return 1;
    }
    if (repeat_btn(cur, pressed, PSP_CTRL_LEFT, &hold_left)) {
        adjust_setting(a, a->sel, -1);
        return 1;
    }
    if (repeat_btn(cur, pressed, PSP_CTRL_RIGHT, &hold_right)) {
        adjust_setting(a, a->sel, +1);
        return 1;
    }
    if (pressed & PSP_CTRL_CROSS) {
        if (a->sel == SET_TEST) {
            audio_set_volume((int)p->set.volume);
            audio_play(SND_FOCUS_DONE);
        } else if (a->sel == SET_RESET) {
            if (a->reset_armed) {
                pomo_reset_stats(p);
                a->reset_armed = 0;
                do_save(a);
                audio_play(SND_CLICK);
            } else {
                a->reset_armed = 1;
            }
        } else if (a->sel == SET_AUTO || a->sel == SET_SCREEN) {
            adjust_setting(a, a->sel, +1);
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
    u32 res;

    setup_callbacks();

    res = sceRtcGetTickResolution();
    if (res > 0) g_tick_res = res;

    /* Load settings and history from the app's folder on the Memory Stick */
    storage_init(argc > 0 ? argv[0] : NULL);
    pomo_defaults(&a->p);
    a->load_status = storage_load(&a->p);
    a->screen = SCR_TIMER;
    a->save_status = 0;

    gfx_init();
    audio_init();
    audio_set_volume((int)a->p.set.volume);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    last_tick = utc_now();
    last_save = last_tick;
    last_power = last_tick;
    last_battery = 0;
    update_clock(a, last_tick);
    update_battery(a);

    while (!g_exit) {
        u64 now = utc_now();
        int64_t dt = elapsed_us(last_tick, now);
        int ev;
        u32 pressed;

        last_tick = now;
        update_clock(a, now);

        /* advance the timer and react to finished phases */
        ev = pomo_tick(&a->p, dt, a->today);
        if (ev & EV_FOCUS_DONE)
            audio_play((ev & EV_LONG_NEXT) ? SND_LONG_BREAK : SND_FOCUS_DONE);
        else if (ev & EV_BREAK_DONE)
            audio_play(SND_BREAK_DONE);
        if (ev & (EV_FOCUS_DONE | EV_BREAK_DONE)) {
            do_save(a);
            last_save = now;
            dirty = 1;
        }

        /* save progress every minute while the timer runs */
        if (a->p.running && elapsed_us(last_save, now) >= 60LL * US_PER_SEC) {
            do_save(a);
            last_save = now;
        }

        /* keep the PSP from auto-sleeping while the timer runs */
        if (a->p.running && elapsed_us(last_power, now) >= 5LL * US_PER_SEC) {
            scePowerTick(a->p.set.keep_screen ? POWER_TICK_ALL : POWER_TICK_SUSPEND);
            last_power = now;
        }

        if (last_battery == 0 || elapsed_us(last_battery, now) >= 10LL * US_PER_SEC) {
            update_battery(a);
            last_battery = now;
        }

        /* input */
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev_buttons;
        if (handle_input(a, pad.Buttons, pressed))
            dirty = 1;
        prev_buttons = pad.Buttons;

        /* redraw 10 times a second, or right away after input */
        if (dirty || (frame % 6) == 0) {
            if (a->screen == SCR_TIMER)      draw_timer(a, frame);
            else if (a->screen == SCR_STATS) draw_stats(a);
            else                             draw_settings(a);
            gfx_present();
            dirty = 0;
        } else {
            gfx_wait_vblank();
        }
        frame++;
    }

    /* Quitting: if we were in settings, apply them; then save everything. */
    if (a->screen == SCR_SETTINGS)
        pomo_settings_changed(&a->p, &a->set_before);
    storage_save(&a->p);
    audio_shutdown();
    sceKernelExitGame();
    return 0;
}
