/*
 * core.c - Pomodoro timer state machine and statistics.
 */
#include "core.h"
#include <string.h>
#include "datetime.h"

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
    s->daily_goal  = clampu(s->daily_goal, 4, 24);
}

static void clear_history(Pomo *p)
{
    int i;
    for (i = 0; i < HIST_DAYS; i++) {
        memset(&p->days[i], 0, sizeof(DayRec));
        p->days[i].day = -1;
        memset(p->days[i].hour, HOUR_UNKNOWN, DAY_HOURS);
    }
}

void pomo_defaults(Pomo *p)
{
    memset(p, 0, sizeof(*p));
    p->set.focus_min   = 25;
    p->set.short_min   = 5;
    p->set.long_min    = 15;
    p->set.long_every  = 4;
    p->set.auto_start  = 0;        /* you confirm each phase with X */
    p->set.volume      = VOL_HIGH;
    p->set.keep_screen = 0;
    p->set.daily_goal  = 16;
    clear_history(p);
    p->phase = PH_FOCUS;
    p->remaining_us = pomo_phase_len_us(p, PH_FOCUS);
}

static int64_t phase_len(const Settings *s, uint32_t phase)
{
    uint32_t min;
    switch (phase) {
    case PH_SHORT: min = s->short_min; break;
    case PH_LONG:  min = s->long_min;  break;
    default:       min = s->focus_min; break;
    }
    return (int64_t)min * 60 * US_PER_SEC;
}

int64_t pomo_phase_len_us(const Pomo *p, uint32_t phase)
{
    return phase_len(&p->set, phase);
}

int pomo_remaining_sec(const Pomo *p)
{
    if (p->remaining_us <= 0) return 0;
    return (int)((p->remaining_us + US_PER_SEC - 1) / US_PER_SEC);
}

float pomo_progress(const Pomo *p)
{
    int64_t len = pomo_phase_len_us(p, p->phase);
    float f;
    if (len <= 0) return 0.0f;
    f = 1.0f - (float)p->remaining_us / (float)len;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

int pomo_not_started(const Pomo *p)
{
    return !p->running && p->remaining_us == pomo_phase_len_us(p, p->phase);
}

/* ---------------- statistics ---------------- */

static int ring_index(int32_t day)
{
    int i = (int)(day % HIST_DAYS);
    return i < 0 ? i + HIST_DAYS : i;
}

const DayRec *pomo_find_day(const Pomo *p, int32_t day)
{
    const DayRec *r = &p->days[ring_index(day)];
    return (r->day == day) ? r : 0;
}

DayRec *pomo_day(Pomo *p, int32_t day)
{
    DayRec *r = &p->days[ring_index(day)];
    if (r->day != day) {
        /* this slot held a day from ~3 years ago (or nothing): reuse it */
        memset(r, 0, sizeof(*r));
        r->day = day;
        memset(r->hour, HOUR_UNKNOWN, DAY_HOURS);
    }
    return r;
}

int pomo_floors(const Pomo *p, int32_t day)
{
    const DayRec *r = pomo_find_day(p, day);
    return r ? (int)r->sessions : 0;
}

int32_t pomo_newest_day(const Pomo *p)
{
    int32_t best = -1;
    int i;
    for (i = 0; i < HIST_DAYS; i++)
        if (p->days[i].day > best) best = p->days[i].day;
    return best;
}

uint32_t pomo_streak(const Pomo *p, int32_t today)
{
    uint32_t streak = 0;
    int32_t d = today;
    if (pomo_floors(p, d) == 0) d--;     /* nothing yet today: yesterday still counts */
    while (pomo_floors(p, d) > 0 && streak < HIST_DAYS) {
        streak++;
        d--;
    }
    return streak;
}

uint32_t pomo_best_streak(const Pomo *p)
{
    int32_t newest = pomo_newest_day(p), d;
    uint32_t best = 0, cur = 0;
    if (newest < 0) return 0;
    for (d = newest - HIST_DAYS + 1; d <= newest; d++) {
        if (pomo_floors(p, d) > 0) { cur++; if (cur > best) best = cur; }
        else cur = 0;
    }
    return best;
}

uint32_t pomo_roof_days(const Pomo *p)
{
    uint32_t n = 0;
    int i;
    for (i = 0; i < HIST_DAYS; i++)
        if (p->days[i].day >= 0 && p->days[i].sessions >= p->set.daily_goal)
            n++;
    return n;
}

void pomo_range_totals(const Pomo *p, int32_t first_day, int32_t last_day,
                       uint32_t *sessions, uint32_t *focus_sec)
{
    uint32_t s = 0, f = 0;
    int32_t d;
    for (d = first_day; d <= last_day; d++) {
        const DayRec *r = pomo_find_day(p, d);
        if (r) { s += r->sessions; f += r->focus_sec; }
    }
    if (sessions) *sessions = s;
    if (focus_sec) *focus_sec = f;
}

uint32_t pomo_check_unlocks(Pomo *p, int32_t today)
{
    uint32_t now = p->unlocked;
    uint32_t streak = pomo_streak(p, today);
    uint32_t best = pomo_best_streak(p);
    uint32_t roofs = pomo_roof_days(p);
    uint32_t added;

    if (streak >= 3 || best >= 3) now |= UNLOCK_TREE_GREEN;
    if (roofs >= 1)               now |= UNLOCK_TREE_ORANGE;
    if (streak >= 7 || best >= 7) now |= UNLOCK_TREE_PURPLE;
    if (roofs >= 5)               now |= UNLOCK_BUS;

    added = now & ~p->unlocked;
    p->unlocked = now;
    return added;
}

void pomo_reset_stats(Pomo *p)
{
    p->total_sessions = 0;
    p->total_focus_sec = 0;
    p->focus_carry_us = 0;
    p->unlocked = 0;
    clear_history(p);
}

/* ---------------- timer ---------------- */

static void set_phase(Pomo *p, uint32_t phase)
{
    p->phase = phase;
    p->remaining_us = pomo_phase_len_us(p, phase);
}

static int complete_phase(Pomo *p, int32_t today, int hour)
{
    int ev;
    if (p->phase == PH_FOCUS) {
        DayRec *r = pomo_day(p, today);
        if (r->sessions < DAY_HOURS)
            r->hour[r->sessions] = (uint8_t)(hour >= 0 && hour < 24 ? hour : HOUR_UNKNOWN);
        if (r->sessions < 0xFFFF) r->sessions++;
        p->total_sessions++;
        p->cycle_done++;
        ev = EV_FOCUS_DONE;
        if (r->sessions == p->set.daily_goal) ev |= EV_ROOF;
        if (p->cycle_done >= p->set.long_every) {
            set_phase(p, PH_LONG);
            ev |= EV_LONG_NEXT;
        } else {
            set_phase(p, PH_SHORT);
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

int pomo_tick(Pomo *p, int64_t delta_us, int32_t today, int hour)
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
        ev |= complete_phase(p, today, hour);

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
        /* Skipping focus does not build a floor. */
        set_phase(p, p->cycle_done >= p->set.long_every ? PH_LONG : PH_SHORT);
    } else {
        if (p->phase == PH_LONG)
            p->cycle_done = 0;
        set_phase(p, PH_FOCUS);
    }
    p->running = 0;   /* the next phase waits for you to press X */
}

void pomo_settings_changed(Pomo *p, const Settings *old)
{
    int64_t old_len;

    pomo_clamp_settings(&p->set);
    old_len = phase_len(old, p->phase);

    /* If the current phase hasn't started, apply the new length now;
       otherwise it applies from the next phase. */
    if (!p->running && p->remaining_us == old_len)
        p->remaining_us = pomo_phase_len_us(p, p->phase);

    if (p->remaining_us > pomo_phase_len_us(p, p->phase))
        p->remaining_us = pomo_phase_len_us(p, p->phase);
}

/* ---------------- monthly seasons ---------------- */

const uint32_t SEASON_THRESHOLD[SEASON_TIERS] = { 0, 40, 100, 180, 260 };

uint32_t pomo_month_floors(const Pomo *p, int year, int month)
{
    int ny = month == 12 ? year + 1 : year, nm = month == 12 ? 1 : month + 1;
    uint32_t s = 0;
    pomo_range_totals(p, dt_day_number(year, month, 1), dt_day_number(ny, nm, 1) - 1, &s, 0);
    return s;
}

int pomo_season_tier(uint32_t n)
{
    int t = 0;
    while (t + 1 < SEASON_TIERS && n >= SEASON_THRESHOLD[t + 1]) t++;
    return t;
}

int pomo_day_tier(const Pomo *p, int32_t day)
{
    int y, m;
    dt_from_day_number(day, &y, &m, 0);
    return pomo_season_tier(pomo_month_floors(p, y, m));
}
