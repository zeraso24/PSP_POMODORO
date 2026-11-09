/*
 * Pomodoro City for PSP
 *
 * A focus timer where every completed session builds a floor of the day's
 * building. Saves to the Memory Stick. Built on the official PSPDEV template.
 * Building art: "2D Pixel City Pack" by laavis (CC-BY 4.0).
 *
 * Timer controls:
 *   X        start / pause (every phase waits for you to press X)
 *   SQUARE   skip to the next phase (no floor is built)
 *   TRIANGLE restart the current phase
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
#include "audio.h"
#include "storage.h"
#include "datetime.h"

PSP_MODULE_INFO("Pomodoro", 0, 2, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

/* scePowerTick() types */
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

enum {
    SET_FOCUS = 0, SET_SHORT, SET_LONG, SET_EVERY, SET_GOAL, SET_AUTO,
    SET_SOUND, SET_SCREEN, SET_TEST, SET_CREDITS, SET_RESET, SET_COUNT
};

static App g_app;
static volatile int g_exit = 0;
static u32 g_tick_res = 1000000;
static int g_msg_t = 0;              /* ticks to show the "save converted" note */

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
/* Time and power                                                       */
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
    u64 secs = local_from_utc(utc) / g_tick_res;
    u64 sec_of_day = secs % 86400ULL;
    a->today = (int32_t)(secs / 86400ULL);
    a->clock_h = (int)(sec_of_day / 3600ULL);
    a->clock_m = (int)((sec_of_day / 60ULL) % 60ULL);
    a->minute_of_day = a->clock_h * 60 + a->clock_m;
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
    if (fast) scePowerSetClockFrequency(333, 333, 166);   /* smooth city animations */
    else      scePowerSetClockFrequency(222, 222, 111);   /* default, saves battery */
}

/* ------------------------------------------------------------------ */
/* Drawing helpers                                                      */
/* ------------------------------------------------------------------ */

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

static int help_icon(int x, int icon, const char *label)
{
    x = gfx_icon(x, 256, 1, icon);
    x = gfx_text(x + 4, 256, 1, COL_DIM, label);
    return x + 14;
}

static int help_text(int x, const char *key, const char *label)
{
    x = gfx_text(x, 256, 1, COL_TEXT, key);
    x = gfx_text(x + 6, 256, 1, COL_DIM, label);
    return x + 14;
}

static void center_left(int y, int scale, uint32_t color, const char *s)
{
    gfx_text(156 - gfx_text_width(s, scale) / 2, y, scale, color, s);
}

/* ------------------------------------------------------------------ */
/* Timer screen                                                         */
/* ------------------------------------------------------------------ */

static void draw_timer(App *a, int frame)
{
    Pomo *p = &a->p;
    int goal = (int)p->set.daily_goal;
    int floors = pomo_floors(p, a->today);
    int secs = pomo_remaining_sec(p);
    int waiting = pomo_not_started(p);
    uint32_t accent = p->phase == PH_FOCUS ? COL_FOCUS : (p->phase == PH_SHORT ? COL_SHORT : COL_LONG);
    const char *label;
    char buf[64], dur[24];
    const DayRec *td = pomo_find_day(p, a->today);
    int x;

    gfx_clear(COL_BG);

    views_timer_window(a, 312, 34, 160, 198);

    gfx_text(16, 12, 2, COL_DIM, "POMODORO");
    if (a->battery >= 0)
        snprintf(buf, sizeof(buf), "%02d:%02d %3d%%%s", a->clock_h, a->clock_m, a->battery,
                 a->charging ? "+" : "");
    else
        snprintf(buf, sizeof(buf), "%02d:%02d", a->clock_h, a->clock_m);
    gfx_text_right(464, 12, 2, COL_DIM, buf);

    if (p->phase == PH_FOCUS) label = "FOCUS";
    else if (waiting) label = "BREAK TIME";
    else label = p->phase == PH_SHORT ? "SHORT BREAK" : "LONG BREAK";
    center_left(44, 3, accent, label);

    snprintf(buf, sizeof(buf), "%02d:%02d", secs / 60, secs % 60);
    center_left(80, 7, p->running ? COL_TEXT : COL_DIM, buf);

    gfx_rect(16, 148, 280, 6, COL_PANEL);
    gfx_rect(16, 148, (int)(280.0f * pomo_progress(p)), 6, accent);

    if (p->phase == PH_FOCUS) {
        int next = floors + 1;
        if (p->running) {
            snprintf(buf, sizeof(buf), "BUILDING FLOOR %d", next);
            center_left(166, 2, COL_TEXT, buf);
            center_left(188, 1, COL_DIM, "The crane lowers it as you focus");
        } else if (waiting) {
            center_left(166, 2, COL_TEXT, "Press X to start");
            if (next > goal) snprintf(buf, sizeof(buf), "Bonus floor %d", next);
            else snprintf(buf, sizeof(buf), "Floor %d of %d", next, goal);
            center_left(188, 1, COL_DIM, buf);
        } else {
            if ((frame / 30) % 2 == 0) center_left(166, 2, accent, "PAUSED");
            snprintf(buf, sizeof(buf), "Floor %d is waiting for you", next);
            center_left(188, 1, COL_DIM, buf);
        }
    } else {
        if (waiting) {
            if ((frame / 30) % 2 == 0 || floors == 0) center_left(166, 2, accent, "Press X to start");
            if (floors >= goal && floors == goal) snprintf(buf, sizeof(buf), "Roof reached! Day complete!");
            else if (floors > goal) snprintf(buf, sizeof(buf), "Bonus floor built!");
            else snprintf(buf, sizeof(buf), "Floor %d built! %d to the roof.", floors, goal - floors);
            center_left(188, 1, COL_TEXT, buf);
        } else {
            center_left(166, 2, COL_DIM, "TAKE A BREAK");
            center_left(188, 1, COL_DIM, "The crane waits for your next floor");
        }
    }

    if (g_msg_t > 0) {
        gfx_text(16, 214, 1, COL_SHORT, "Save updated to v2 - history kept");
    } else if (a->unlock_t > 0 && a->new_unlocks) {
        gfx_text(16, 214, 1, RGB(255, 170, 90), "New unlock! See your street (SELECT)");
    }

    fmt_duration(dur, sizeof(dur), td ? td->focus_sec : 0);
    snprintf(buf, sizeof(buf), "Today: %d floors - %s", floors, dur);
    gfx_text(16, 234, 1, COL_DIM, buf);
    if (a->save_status < 0) gfx_text_right(296, 234, 1, COL_WARN, "SAVE FAILED");
    else if (a->save_status > 0) gfx_text_right(296, 234, 1, COL_DIM, "Saved");

    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    x = 12;
    x = help_icon(x, ICON_CROSS, p->running ? "Pause" : "Start");
    x = help_icon(x, ICON_SQUARE, "Skip");
    x = help_icon(x, ICON_TRIANGLE, "Restart");
    x = help_text(x, "SELECT", "City");
    help_text(x, "START", "Settings");
}

/* ------------------------------------------------------------------ */
/* Settings and credits                                                 */
/* ------------------------------------------------------------------ */

static const char *VOLUME_NAMES[4] = { "Off", "Low", "Medium", "High" };

static void settings_value(App *a, int item, char *buf, size_t n)
{
    Settings *s = &a->p.set;
    switch (item) {
    case SET_FOCUS:   snprintf(buf, n, "< %u min >", (unsigned)s->focus_min); break;
    case SET_SHORT:   snprintf(buf, n, "< %u min >", (unsigned)s->short_min); break;
    case SET_LONG:    snprintf(buf, n, "< %u min >", (unsigned)s->long_min); break;
    case SET_EVERY:   snprintf(buf, n, "< %u sessions >", (unsigned)s->long_every); break;
    case SET_GOAL:    snprintf(buf, n, "< %u floors >", (unsigned)s->daily_goal); break;
    case SET_AUTO:    snprintf(buf, n, "< %s >", s->auto_start ? "On" : "Off"); break;
    case SET_SOUND:   snprintf(buf, n, "< %s >", VOLUME_NAMES[s->volume & 3]); break;
    case SET_SCREEN:  snprintf(buf, n, "< %s >", s->keep_screen ? "On" : "Off"); break;
    case SET_TEST:    snprintf(buf, n, "Press X"); break;
    case SET_CREDITS: snprintf(buf, n, "Press X"); break;
    case SET_RESET:   snprintf(buf, n, a->reset_armed ? "Press X again" : "Press X"); break;
    default:          buf[0] = '\0'; break;
    }
}

static void draw_settings(App *a)
{
    static const char *labels[SET_COUNT] = {
        "Focus", "Short break", "Long break", "Long after", "Daily goal", "Auto-start",
        "Sound", "Keep screen", "Test sound", "Credits", "Reset stats"
    };
    char buf[32];
    int i, x;

    gfx_clear(COL_BG);
    gfx_text(16, 10, 2, COL_DIM, "SETTINGS");

    for (i = 0; i < SET_COUNT; i++) {
        int y = 34 + i * 19;
        int selected = (i == a->sel);
        uint32_t vcol = COL_TEXT;
        if (selected) {
            gfx_rect(12, y - 2, 456, 19, COL_PANEL);
            gfx_rect(12, y - 2, 3, 19, COL_FOCUS);
        }
        gfx_text(24, y, 2, selected ? COL_TEXT : COL_DIM, labels[i]);
        settings_value(a, i, buf, sizeof(buf));
        if (i == SET_RESET && a->reset_armed) vcol = COL_WARN;
        else if (!selected) vcol = COL_DIM;
        gfx_text_right(460, y, 2, vcol, buf);
    }

    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    if (a->reset_armed) {
        gfx_text(12, 256, 1, COL_WARN, "Erases ALL your history and buildings. X again to confirm.");
        return;
    }
    x = 12;
    x = help_text(x, "UP/DOWN", "Select");
    x = help_text(x, "LEFT/RIGHT", "Change");
    x = help_icon(x, ICON_CROSS, "Action");
    help_icon(x, ICON_CIRCLE, "Back");
}

static void draw_credits(void)
{
    gfx_clear(COL_BG);
    gfx_text(16, 12, 2, COL_DIM, "CREDITS");
    gfx_text(24, 50, 2, COL_TEXT, "Pomodoro City for PSP");
    gfx_text(24, 70, 1, COL_DIM, "Version 2");
    gfx_text(24, 100, 1, COL_TEXT, "Building, street and sky art:");
    gfx_text(24, 114, 2, RGB(255, 170, 90), "2D Pixel City Pack");
    gfx_text(24, 134, 1, COL_TEXT, "by laavis - licensed CC-BY 4.0");
    gfx_text(24, 148, 1, COL_DIM, "opengameart.org/content/2d-pixel-city-pack");
    gfx_text(24, 178, 1, COL_TEXT, "Built with the PSPDEV toolchain");
    gfx_text(24, 192, 1, COL_DIM, "pspdev.github.io");
    gfx_rect(0, 248, SCREEN_W, 1, COL_PANEL);
    help_icon(12, ICON_CIRCLE, "Back");
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

static int handle_input(App *a, u32 cur, u32 pressed)
{
    static int hold_up, hold_down, hold_left, hold_right;
    Pomo *p = &a->p;

    switch (a->screen) {
    case SCR_TIMER:
        if (pressed & PSP_CTRL_CROSS) {
            pomo_toggle(p);
            audio_play(SND_CLICK);
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
        if (pressed & PSP_CTRL_SELECT) {
            a->screen = SCR_CITY;
            views_open(a, VIEW_TODAY);
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

    case SCR_CITY:
        return views_input(a, cur, pressed);

    case SCR_CREDITS:
        if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_START)) {
            a->screen = SCR_SETTINGS;
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
        } else if (a->sel == SET_CREDITS) {
            a->screen = SCR_CREDITS;
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

    memset(a, 0, sizeof(*a));
    a->land_t = -1;
    a->celebrate = -1;

    storage_init(argc > 0 ? argv[0] : NULL);
    pomo_defaults(&a->p);
    a->load_status = storage_load(&a->p);
    if (a->load_status == 2) g_msg_t = 30 * 10;

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
    pomo_check_unlocks(&a->p, a->today);   /* quietly, at start-up */
    if (a->load_status == 2) do_save(a);   /* store the converted save */

    while (!g_exit) {
        u64 now = utc_now();
        int64_t dt = elapsed_us(last_tick, now);
        int ev, render;
        u32 pressed;

        last_tick = now;
        update_clock(a, now);

        /* animation clock: 30 ticks per second */
        if (frame % 2 == 0) {
            a->anim++;
            if (a->land_t >= 0 && ++a->land_t > 60) a->land_t = -1;
            if (a->celebrate >= 0 && ++a->celebrate > 200) {
                a->celebrate = -1;
                a->cam_follow = 0;
                a->cam = 0;
            }
            if (a->unlock_t > 0 && a->celebrate < 0) a->unlock_t--;
            if (g_msg_t > 0) g_msg_t--;
        }

        /* advance the timer and react to finished phases */
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
        } else if (ev & EV_BREAK_DONE) {
            audio_play(SND_BREAK_DONE);
        }
        if (ev & (EV_FOCUS_DONE | EV_BREAK_DONE)) {
            do_save(a);
            last_save = now;
            dirty = 1;
        }

        /* save progress every two minutes while the timer runs */
        if (a->p.running && elapsed_us(last_save, now) >= 120LL * US_PER_SEC) {
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

        /* city: 30 fps; timer: 15 fps; menus: when something changes */
        set_fast_cpu(a->screen == SCR_CITY);
        if (a->screen == SCR_CITY)       render = dirty || (frame % 2) == 0;
        else if (a->screen == SCR_TIMER) render = dirty || (frame % 4) == 0;
        else                             render = dirty || (frame % 30) == 0;

        if (render) {
            switch (a->screen) {
            case SCR_TIMER:    draw_timer(a, frame); break;
            case SCR_CITY:     views_draw(a);        break;
            case SCR_SETTINGS: draw_settings(a);     break;
            default:           draw_credits();       break;
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
