/*
 * datetime.c - calendar helpers (proleptic Gregorian calendar).
 * Uses Howard Hinnant's civil_from_days algorithm.
 */
#include "datetime.h"

/* Day number of 1970-01-01 when day 0 is 0001-01-01. */
#define DAYS_0001_TO_1970 719162

void dt_from_day_number(int32_t day_number, int *year, int *month, int *day)
{
    int64_t z = (int64_t)day_number - DAYS_0001_TO_1970; /* days since 1970-01-01 */
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;                                   /* [0, 146096] */
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; /* [0, 399] */
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);            /* [0, 365] */
    int64_t mp = (5 * doy + 2) / 153;                                 /* [0, 11] */
    int64_t d = doy - (153 * mp + 2) / 5 + 1;                         /* [1, 31] */
    int64_t m = mp < 10 ? mp + 3 : mp - 9;                            /* [1, 12] */
    if (m <= 2) y += 1;

    if (year) *year = (int)y;
    if (month) *month = (int)m;
    if (day) *day = (int)d;
}

int dt_weekday(int32_t day_number)
{
    /* 0001-01-01 was a Monday in the proleptic Gregorian calendar. */
    int w = (int)(day_number % 7);
    if (w < 0) w += 7;
    return w;
}

const char *dt_weekday_name(int32_t day_number)
{
    static const char *names[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    return names[dt_weekday(day_number)];
}

int32_t dt_day_number(int year, int month, int day)
{
    int64_t y = year - (month <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t mp = month > 2 ? month - 3 : month + 9;
    int64_t doy = (153 * mp + 2) / 5 + day - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days_1970 = era * 146097 + doe - 719468;
    return (int32_t)(days_1970 + DAYS_0001_TO_1970);
}

const char *dt_month_name(int month)
{
    static const char *names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    if (month < 1 || month > 12) return "???";
    return names[month - 1];
}
