/*
 * app.h - state shared by the main loop (timer, settings) and the city views.
 */
#ifndef POMO_APP_H
#define POMO_APP_H

#include <stdint.h>
#include "core.h"

enum { SCR_TIMER = 0, SCR_CITY, SCR_SETTINGS, SCR_CREDITS };
enum { VIEW_TODAY = 0, VIEW_WEEK, VIEW_MONTH, VIEW_YEAR, VIEW_COUNT };

typedef struct {
    Pomo     p;
    int      screen;
    int      anim;            /* animation clock, 30 ticks per second */

    /* clock and power */
    int32_t  today;           /* local day number */
    int      clock_h, clock_m;
    int      minute_of_day;
    int      battery;         /* percent, <0 if unknown */
    int      charging;

    /* saving */
    int      save_status;     /* 1 ok, -1 failed, 0 not saved yet */
    int      load_status;     /* storage_load() result */

    /* settings screen */
    int      sel;
    int      reset_armed;
    Settings set_before;

    /* timer window */
    int      land_t;          /* ticks since a floor landed, -1 = none */

    /* city views */
    int      view;
    int32_t  sel_day;         /* Today: day shown; Year: cursor */
    int32_t  week_start;      /* Monday of the week shown */
    int      month_y, month_m;
    int      cam;             /* Today: how far the camera climbed (px) */
    int      cam_follow;      /* 1 = camera follows the top of the tower */
    int      celebrate;       /* ticks since the roof celebration began, -1 = none */
    uint32_t new_unlocks;     /* UNLOCK_* bits to announce */
    int      unlock_t;        /* ticks left to show the unlock message */
} App;

/* views.c */
void views_open(App *a, int view);
void views_start_celebration(App *a);
int  views_input(App *a, uint32_t cur, uint32_t pressed);   /* 1 = redraw */
void views_draw(App *a);
void views_timer_window(App *a, int x, int y, int w, int h);
int  views_style_for_day(int32_t day);

#endif
