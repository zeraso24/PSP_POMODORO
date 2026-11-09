/*
 * core.h - Pomodoro timer state machine and statistics.
 *
 * No PSP-specific code lives here: the main loop feeds it elapsed time and
 * today's day number, and it reports events (phase finished, time credited).
 * That keeps the logic easy to test on a computer.
 */
#ifndef POMO_CORE_H
#define POMO_CORE_H

#include <stdint.h>

#define HIST_DAYS 60          /* days of history kept in the save file */
#define US_PER_SEC 1000000LL

enum { PH_FOCUS = 0, PH_SHORT = 1, PH_LONG = 2 };

/* Sound volume levels */
enum { VOL_OFF = 0, VOL_LOW = 1, VOL_MED = 2, VOL_HIGH = 3 };

/* Events returned by pomo_tick() and the action functions (bit flags). */
#define EV_FOCUS_DONE    0x01  /* a focus session finished: a break starts */
#define EV_LONG_NEXT     0x02  /* ...and the break is the long one */
#define EV_BREAK_DONE    0x04  /* a break finished: focus starts */
#define EV_TIME_CREDITED 0x08  /* focus seconds were added to the stats */

typedef struct {
    uint32_t focus_min;    /* 1..90 */
    uint32_t short_min;    /* 1..30 */
    uint32_t long_min;     /* 1..60 */
    uint32_t long_every;   /* long break after N focus sessions, 2..8 */
    uint32_t auto_start;   /* 1 = next phase starts by itself */
    uint32_t volume;       /* VOL_OFF..VOL_HIGH */
    uint32_t keep_screen;  /* 1 = keep the backlight on while running */
} Settings;

typedef struct {
    int32_t  day;          /* day number (days since 0001-01-01), -1 = empty */
    uint32_t sessions;     /* completed focus sessions that day */
    uint32_t focus_sec;    /* seconds spent in focus that day (incl. partial) */
} DayRec;

typedef struct {
    Settings set;

    /* statistics */
    uint32_t total_sessions;
    uint32_t total_focus_sec;
    DayRec   days[HIST_DAYS];  /* days[0] is the most recent day */

    /* timer */
    uint32_t phase;            /* PH_FOCUS / PH_SHORT / PH_LONG */
    uint32_t cycle_done;       /* focus sessions done in the current cycle */
    int      running;          /* 1 = counting down */
    int64_t  remaining_us;     /* time left in the current phase */
    int64_t  focus_carry_us;   /* sub-second focus time not yet credited */
} Pomo;

void     pomo_defaults(Pomo *p);
void     pomo_clamp_settings(Settings *s);
int64_t  pomo_phase_len_us(const Pomo *p, uint32_t phase);
int      pomo_remaining_sec(const Pomo *p);   /* rounded up, for display */
float    pomo_progress(const Pomo *p);        /* 0.0 .. 1.0 of current phase */

/* Advance the timer by delta_us of real time. Returns EV_* flags. */
int      pomo_tick(Pomo *p, int64_t delta_us, int32_t today);

/* User actions */
void     pomo_toggle(Pomo *p);                /* start / pause */
void     pomo_reset_phase(Pomo *p);           /* restart current phase, paused */
void     pomo_skip(Pomo *p);                  /* jump to next phase, no credit */
void     pomo_settings_changed(Pomo *p, const Settings *old);
void     pomo_reset_stats(Pomo *p);

/* Statistics helpers */
DayRec        *pomo_day(Pomo *p, int32_t day);              /* find or create */
const DayRec  *pomo_find_day(const Pomo *p, int32_t day);   /* NULL if none */
uint32_t       pomo_streak(const Pomo *p, int32_t today);   /* days in a row */
void           pomo_range_totals(const Pomo *p, int32_t first_day, int32_t last_day,
                                 uint32_t *sessions, uint32_t *focus_sec);

#endif
