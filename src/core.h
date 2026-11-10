/*
 * core.h - Pomodoro timer state machine and statistics.
 *
 * No PSP-specific code lives here: the main loop feeds it elapsed time,
 * today's day number and the current hour, and it reports events.
 * That keeps the logic easy to test on a computer.
 *
 * Each completed focus session is one floor of the day's building.
 */
#ifndef POMO_CORE_H
#define POMO_CORE_H

#include <stdint.h>

#define HIST_DAYS 1100        /* about 3 years of history, stored as a ring */
#define DAY_HOURS 24          /* hours kept per day (one per floor) */
#define HOUR_UNKNOWN 0xFF
#define US_PER_SEC 1000000LL

enum { PH_FOCUS = 0, PH_SHORT = 1, PH_LONG = 2 };

/* Sound volume levels */
enum { VOL_OFF = 0, VOL_LOW = 1, VOL_MED = 2, VOL_HIGH = 3 };

/* Things you unlock for your street (bit flags) */
enum {
    UNLOCK_TREE_GREEN  = 1,   /* 3-day streak */
    UNLOCK_TREE_ORANGE = 2,   /* first day with a roof */
    UNLOCK_TREE_PURPLE = 4,   /* 7-day streak */
    UNLOCK_BUS         = 8    /* 5 days with a roof */
};

/* Events returned by pomo_tick() (bit flags). */
#define EV_FOCUS_DONE    0x01  /* a focus session finished: a floor was built */
#define EV_LONG_NEXT     0x02  /* ...and the break is the long one */
#define EV_BREAK_DONE    0x04  /* a break finished: focus is next */
#define EV_TIME_CREDITED 0x08  /* focus seconds were added to the stats */
#define EV_ROOF          0x10  /* this floor reached the daily goal */

typedef struct {
    uint32_t focus_min;    /* 1..90 */
    uint32_t short_min;    /* 1..30 */
    uint32_t long_min;     /* 1..60 */
    uint32_t long_every;   /* long break after N focus sessions, 2..8 */
    uint32_t auto_start;   /* 1 = next phase starts by itself */
    uint32_t volume;       /* VOL_OFF..VOL_HIGH */
    uint32_t keep_screen;  /* 1 = keep the backlight on while running */
    uint32_t daily_goal;   /* floors for the roof, 4..24 */
} Settings;

typedef struct {
    int32_t  day;              /* day number (days since 0001-01-01), -1 = empty */
    uint16_t sessions;         /* completed focus sessions = floors */
    uint16_t reserved;
    uint32_t focus_sec;        /* seconds spent in focus (incl. partial) */
    uint8_t  hour[DAY_HOURS];  /* hour each floor was finished */
} DayRec;

typedef struct {
    Settings set;

    /* statistics */
    uint32_t total_sessions;
    uint32_t total_focus_sec;
    uint32_t unlocked;         /* UNLOCK_* bits */
    DayRec   days[HIST_DAYS];  /* ring buffer: a day lives at days[day % HIST_DAYS] */

    /* timer */
    uint32_t phase;            /* PH_FOCUS / PH_SHORT / PH_LONG */
    uint32_t cycle_done;       /* focus sessions done in the current cycle */
    int      running;          /* 1 = counting down */
    int64_t  remaining_us;     /* time left in the current phase */
    int64_t  focus_carry_us;   /* sub-second focus time not yet credited */

    /* manual fix for a PSP whose clock is off (minutes, -720..720) */
    int32_t  clock_offset_min;
} Pomo;

void     pomo_defaults(Pomo *p);
void     pomo_clamp_settings(Settings *s);
int64_t  pomo_phase_len_us(const Pomo *p, uint32_t phase);
int      pomo_remaining_sec(const Pomo *p);   /* rounded up, for display */
float    pomo_progress(const Pomo *p);        /* 0.0 .. 1.0 of current phase */
int      pomo_not_started(const Pomo *p);     /* paused at the full length */

/* Advance the timer by delta_us of real time. Returns EV_* flags. */
int      pomo_tick(Pomo *p, int64_t delta_us, int32_t today, int hour);

/* User actions */
void     pomo_toggle(Pomo *p);                /* start / pause */
void     pomo_reset_phase(Pomo *p);           /* restart current phase, paused */
void     pomo_skip(Pomo *p);                  /* jump to next phase, no credit */
void     pomo_settings_changed(Pomo *p, const Settings *old);
void     pomo_reset_stats(Pomo *p);

/* Statistics helpers */
DayRec        *pomo_day(Pomo *p, int32_t day);              /* find or create */
const DayRec  *pomo_find_day(const Pomo *p, int32_t day);   /* NULL if none */
int            pomo_floors(const Pomo *p, int32_t day);     /* 0 if none */
uint32_t       pomo_streak(const Pomo *p, int32_t today);   /* days in a row */
uint32_t       pomo_best_streak(const Pomo *p);
uint32_t       pomo_roof_days(const Pomo *p);
void           pomo_range_totals(const Pomo *p, int32_t first_day, int32_t last_day,
                                 uint32_t *sessions, uint32_t *focus_sec);
int32_t        pomo_newest_day(const Pomo *p);              /* -1 if no history */
/* Checks the unlock rules; returns the bits that were newly unlocked. */
uint32_t       pomo_check_unlocks(Pomo *p, int32_t today);

#endif
