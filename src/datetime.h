/*
 * datetime.h - calendar helpers that work on "day numbers".
 *
 * The PSP real-time clock counts microseconds since 0001-01-01 00:00:00.
 * Dividing a local tick by one day gives a day number where day 0 is
 * 0001-01-01. Everything here is plain C so it can be tested on a computer.
 */
#ifndef POMO_DATETIME_H
#define POMO_DATETIME_H

#include <stdint.h>

/* Converts a day number (days since 0001-01-01) to year, month (1-12), day (1-31). */
void dt_from_day_number(int32_t day_number, int *year, int *month, int *day);

/* Day of the week for a day number: 0 = Monday ... 6 = Sunday. */
int dt_weekday(int32_t day_number);

/* Three-letter English weekday name ("Mon", "Tue", ...). */
const char *dt_weekday_name(int32_t day_number);

#endif
