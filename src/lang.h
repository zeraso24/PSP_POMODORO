/*
 * lang.h - Spanish / English texts.
 *
 * Accented letters use Latin-1 codes; gfx.c draws them with glyphs built at
 * start-up (the PSP's debug font has none). Write them with these macros so
 * a following letter can never be swallowed by a \x escape:
 *   "Sesi" L_o "n"  ->  "Sesión"
 */
#ifndef POMO_LANG_H
#define POMO_LANG_H

#include <stdint.h>

#define L_a "\xe1"
#define L_e "\xe9"
#define L_i "\xed"
#define L_o "\xf3"
#define L_u "\xfa"
#define L_n "\xf1"
#define L_A "\xc1"
#define L_E "\xc9"
#define L_I "\xcd"
#define L_O "\xd3"
#define L_U "\xda"
#define L_N "\xd1"
#define L_Q "\xbf"   /* inverted question mark */
#define L_X "\xa1"   /* inverted exclamation mark */

enum { LANG_ES = 0, LANG_EN = 1, LANG_COUNT };

typedef enum {
    /* timer */
    S_FOCUS, S_BREAK_TIME, S_SHORT_BREAK, S_LONG_BREAK, S_BUILDING_FLOOR, S_CRANE_LOWERS,
    S_PRESS_X, S_BONUS_FLOOR_N, S_FLOOR_OF, S_PAUSED, S_FLOOR_WAITING, S_ROOF_REACHED,
    S_BONUS_BUILT, S_FLOOR_BUILT, S_TAKE_BREAK, S_CRANE_WAITS, S_NEW_UNLOCK_HINT,
    S_TODAY_LINE, S_SAVED, S_SAVE_FAILED,
    /* help bar */
    S_H_START, S_H_PAUSE, S_H_SKIP, S_H_RESTART, S_H_CITY, S_H_SETTINGS, S_H_VIEW,
    S_H_SELECT, S_H_CHANGE, S_H_ACTION, S_H_BACK, S_H_DAY, S_H_LOOK, S_H_WEEK, S_H_MONTH,
    S_H_MOVE, S_H_TIMER, S_H_NEXT,
    /* toasts */
    S_T_NEW_DAY, S_T_CLOCK_CHANGED, S_T_SAVE_CONVERTED, S_T_SEASON_UP, S_T_REMIND,
    /* settings */
    S_SETTINGS, S_SEC_TIMER, S_SEC_CITY, S_SEC_SYSTEM,
    S_SET_FOCUS, S_SET_SHORT, S_SET_LONG, S_SET_EVERY, S_SET_AUTO, S_SET_REMIND,
    S_SET_GOAL, S_SET_VIEW, S_SET_LANG, S_SET_SOUND, S_SET_SCREEN, S_SET_CLOCK,
    S_SET_TEST, S_SET_HOWTO, S_SET_CREDITS, S_SET_RESET,
    S_V_MIN, S_V_SESSIONS, S_V_FLOORS, S_V_ON, S_V_OFF, S_V_PRESS_X, S_V_PRESS_AGAIN,
    S_RESET_WARNING,
    S_VOL_OFF, S_VOL_LOW, S_VOL_MED, S_VOL_HIGH,
    S_VIEW_CLASSIC, S_VIEW_CINEMA, S_VIEW_BIG,
    /* city */
    S_TAB_TODAY, S_TAB_WEEK, S_TAB_MONTH, S_TAB_YEAR,
    S_FLOORS, S_DAY_COMPLETE, S_BONUS_FLOORS, S_CRANE_LOWERING, S_UNTIL_LANDS, S_NEXT_FLOOR,
    S_START_FROM_TIMER, S_BREAK, S_TO_THE_ROOF, S_FLOORS_BUILT, S_NO_FLOORS, S_FOCUS_LABEL,
    S_STREAK, S_NEW_X, S_UNL_BUS, S_UNL_PURPLE, S_UNL_ORANGE, S_UNL_GREEN, S_STYLE,
    S_STYLE_BROWN, S_STYLE_SHOP, S_STYLE_CLUB,
    S_WEEK_LINE, S_MONTH_LINE, S_SEASON_LINE, S_SEASON_MAX,
    S_YEAR_FLOORS, S_YEAR_ROOFS, S_BEST_STREAK, S_NOT_YET, S_DAY_FLOORS, S_DAY_ROOF,
    S_DAY_FOCUS, S_LEG_LOW, S_LEG_HIGH, S_LEG_ROOF,
    S_SEASON_0, S_SEASON_1, S_SEASON_2, S_SEASON_3, S_SEASON_4,
    /* cinema and big clock */
    S_HUD_FLOOR, S_HUD_STREAK, S_BIG_STATE, S_BIG_SAVER,
    /* tutorial */
    S_TUT_TITLE, S_TUT_1A, S_TUT_1B, S_TUT_2A, S_TUT_2B, S_TUT_2C, S_TUT_3A, S_TUT_3B, S_TUT_3C,
    S_TUT_FOOTER,
    /* credits */
    S_CREDITS, S_CR_VERSION, S_CR_BUILDINGS, S_CR_RESIDENTS, S_CR_TOOLCHAIN,
    S_COUNT
} StrId;

void        lang_set(int lang);
int         lang_get(void);
const char *T(StrId id);
const char *lang_weekday(int32_t day_number);     /* "Lun" / "Mon" */
const char *lang_month_short(int month);          /* "ENE" / "JAN" */
const char *lang_month_long(int month);           /* "ENERO" / "JANUARY" */
const char *lang_language_name(int lang);

#endif
