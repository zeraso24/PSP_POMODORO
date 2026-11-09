/*
 * core.c - Pomodoro timer state machine and statistics.
 */
#include "core.h"
#include <string.h>

static uint32_t clampu(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void pomo_clamp_settings(Settings *s)
{
    s->focus_min   = clampu(s->focus_min, 1, 90);
    s->short_min   = clampu(s->short_min, 1, 30);
    s->long_min    = clampu(s->long_min, 1, 60);
    s->long_every  = clampu(s->long_every, 2, 8);
    s->auto_start  = s->auto_start ? 1 : 0;
    s->volume      = clampu(s->volume, VOL_OFF, VOL_HIGH);
    s->keep_screen = s->keep_screen ? 1 : 0;
}

static void clear_history(Pomo *p)
{
    int i;
    for (i = 0; i < HIST_DAYS; i++) {
        p->days[i].day = -1;
        p->days[i].sessions = 0;
        p->days[i].focus_sec = 0;
    }
}

void pomo_defaults(Pomo *p)
{
    memset(p, 0, sizeof(*p));
    p->set.focus_min   = 25;
    p->set.short_min   = 5;
    p->set.long_min    = 15;
    p->set.long_every  = 4;
    p->set.auto_start  = 1;
    p->set.volume      = VOL_HIGH;
    p->set.keep_screen = 0;
    clear_history(p);
    p->phase = PH_FOCUS;
    p->cycle_done = 0;
    p->running = 0;
    p->remaining_us = pomo_phase_len_us(p, PH_FOCUS);
    p->focus_carry_us = 0;
}

int64_t pomo_phase_len_us(const Pomo *p, uint32_t phase)
{
    uint32_t min;
    switch (phase) {
    case PH_SHORT: min = p->set.short_min; break;
    case PH_LONG:  min = p->set.long_min;  break;
    default:       min = p->set.focus_min; break;
    }
    return (int64_t)min * 60 * US_PER_SEC;
}

int pomo_remaining_sec(const Pomo *p)
{
    if (p->remaining_us <= 0) return 0;
    return (int)((p->remaining_us + US_PER_SEC - 1) / US_PER_SEC);
}

float pomo_progress(const Pomo *p)
{
    int64_t len = pomo_phase_len_us(p, p->phase);
    if (len <= 0) return 0.0f;
    float f = 1.0f - (float)p->remaining_us / (float)len;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

/* ---------------- statistics ---------------- */

const DayRec *pomo_find_day(const Pomo *p, int32_t day)
{
    int i;
    for (i = 0; i < HIST_DAYS; i++)
        if (p->days[i].day == day)
            return &p->days[i];
    return 0;
}

DayRec *pomo_day(Pomo *p, int32_t day)
{
    int i;
    for (i = 0; i < HIST_DAYS; i++)
        if (p->days[i].day == day)
            return &p->days[i];

    /* New day: shift history down (oldest falls off) and insert at the top. */
    memmove(&p->days[1], &p->days[0], sizeof(DayRec) * (HIST_DAYS - 1));
    p->days[0].day = day;
    p->days[0].sessions = 0;
    p->days[0].focus_sec = 0;
    return &p->days[0];
}

uint32_t pomo_streak(const Pomo *p, int32_t today)
{
    uint32_t streak = 0;
    int32_t d = today;
    const DayRec *r = pomo_find_day(p, d);

    /* If nothing done yet today, the streak can still be alive from yesterday. */
    if (!r || r->sessions == 0)
        d--;

    for (;;) {
        r = pomo_find_day(p, d);
        if (!r || r->sessions == 0) break;
        streak++;
        d--;
    }
    return streak;
}

void pomo_range_totals(const Pomo *p, int32_t first_day, int32_t last_day,
                       uint32_t *sessions, uint32_t *focus_sec)
{
    int i;
    uint32_t s = 0, f = 0;
    for (i = 0; i < HIST_DAYS; i++) {
        const DayRec *r = &p->days[i];
        if (r->day >= first_day && r->day <= last_day) {
            s += r->sessions;
            f += r->focus_sec;
        }
    }
    if (sessions) *sessions = s;
    if (focus_sec) *focus_sec = f;
}

void pomo_reset_stats(Pomo *p)
{
    p->total_sessions = 0;
    p->total_focus_sec = 0;
    p->focus_carry_us = 0;
    clear_history(p);
}

/* ---------------- timer ---------------- */

static void set_phase(Pomo *p, uint32_t phase)
{
    p->phase = phase;
    p->remaining_us = pomo_phase_len_us(p, phase);
}

static int complete_phase(Pomo *p, int32_t today)
{
    int ev;
    if (p->phase == PH_FOCUS) {
        p->total_sessions++;
        pomo_day(p, today)->sessions++;
        p->cycle_done++;
        if (p->cycle_done >= p->set.long_every) {
            set_phase(p, PH_LONG);
            ev = EV_FOCUS_DONE | EV_LONG_NEXT;
        } else {
            set_phase(p, PH_SHORT);
            ev = EV_FOCUS_DONE;
        }
    } else {
        if (p->phase == PH_LONG)
            p->cycle_done = 0;
        set_phase(p, PH_FOCUS);
        ev = EV_BREAK_DONE;
    }
    p->running = p->set.auto_start ? 1 : 0;
    return ev;
}

int pomo_tick(Pomo *p, int64_t delta_us, int32_t today)
{
    int ev = 0;

    if (!p->running || delta_us <= 0)
        return 0;

    /* If the PSP was asleep for a long time, never credit more than the
       time that was actually left in this phase. */
    if (delta_us > p->remaining_us)
        delta_us = p->remaining_us;

    p->remaining_us -= delta_us;

    if (p->phase == PH_FOCUS) {
        p->focus_carry_us += delta_us;
        if (p->focus_carry_us >= US_PER_SEC) {
            uint32_t secs = (uint32_t)(p->focus_carry_us / US_PER_SEC);
            p->focus_carry_us -= (int64_t)secs * US_PER_SEC;
            pomo_day(p, today)->focus_sec += secs;
            p->total_focus_sec += secs;
            ev |= EV_TIME_CREDITED;
        }
    }

    if (p->remaining_us <= 0)
        ev |= complete_phase(p, today);

    return ev;
}

void pomo_toggle(Pomo *p)
{
    p->running = !p->running;
}

void pomo_reset_phase(Pomo *p)
{
    p->running = 0;
    p->remaining_us = pomo_phase_len_us(p, p->phase);
}

void pomo_skip(Pomo *p)
{
    if (p->phase == PH_FOCUS) {
        /* Skipping focus does not count as a completed session. */
        set_phase(p, p->cycle_done >= p->set.long_every ? PH_LONG : PH_SHORT);
    } else {
        if (p->phase == PH_LONG)
            p->cycle_done = 0;
        set_phase(p, PH_FOCUS);
    }
    /* running state is kept: if it was running, the next phase starts now */
}

void pomo_settings_changed(Pomo *p, const Settings *old)
{
    Pomo tmp;
    int64_t old_len;

    pomo_clamp_settings(&p->set);

    /* What was the length of the current phase with the old settings? */
    tmp = *p;
    tmp.set = *old;
    old_len = pomo_phase_len_us(&tmp, p->phase);

    /* If the current phase hasn't been started yet, apply the new length now.
       Otherwise the new length applies from the next phase. */
    if (!p->running && p->remaining_us == old_len)
        p->remaining_us = pomo_phase_len_us(p, p->phase);

    /* Never leave more time than the phase allows. */
    if (p->remaining_us > pomo_phase_len_us(p, p->phase))
        p->remaining_us = pomo_phase_len_us(p, p->phase);
}
