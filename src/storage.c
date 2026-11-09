/*
 * storage.c - crash-safe save files with a checksum and a backup copy.
 *
 * Every save writes save.bak first and save.dat second, so at any moment at
 * least one complete copy exists, even if the battery dies mid-write.
 */
#include "storage.h"
#include "datetime.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define SAVE_MAGIC 0x52444D50u  /* "PMDR" */

/* ---- version 2 (current) ---- */
typedef struct {
    uint32_t magic;
    uint32_t version;            /* 2 */
    uint32_t size;
    Settings set;
    uint32_t total_sessions;
    uint32_t total_focus_sec;
    uint32_t unlocked;
    uint32_t phase;
    uint32_t cycle_done;
    uint32_t remaining_sec;
    uint32_t reserved[8];
    DayRec   days[HIST_DAYS];
    uint32_t checksum;           /* FNV-1a of everything above */
} SaveBlob;

/* ---- version 1 (first release), read only for conversion ---- */
typedef struct {
    uint32_t focus_min, short_min, long_min, long_every, auto_start, volume, keep_screen;
} SettingsV1;
typedef struct { int32_t day; uint32_t sessions; uint32_t focus_sec; } DayRecV1;
typedef struct {
    uint32_t   magic;
    uint32_t   version;          /* 1 */
    uint32_t   size;
    SettingsV1 set;
    uint32_t   total_sessions, total_focus_sec, phase, cycle_done, remaining_sec;
    uint32_t   reserved[7];
    DayRecV1   days[60];
    uint32_t   checksum;
} SaveBlobV1;

static SaveBlob g_blob;          /* ~40 KB: kept static, not on the stack */
static union { SaveBlob v2; SaveBlobV1 v1; } g_read;

static char g_dir[256];
static char g_path_dat[300];
static char g_path_bak[300];
static char g_path_csv[300];

static uint32_t fnv1a(const void *data, size_t len)
{
    const unsigned char *b = (const unsigned char *)data;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < len; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

void storage_init(const char *argv0)
{
    const char *slash = argv0 ? strrchr(argv0, '/') : NULL;

    if (slash && (size_t)(slash - argv0) < sizeof(g_dir) - 1) {
        size_t n = (size_t)(slash - argv0);
        memcpy(g_dir, argv0, n);
        g_dir[n] = '\0';
    } else {
        strcpy(g_dir, "ms0:/PSP/GAME/Pomodoro");
    }

    snprintf(g_path_dat, sizeof(g_path_dat), "%s/save.dat", g_dir);
    snprintf(g_path_bak, sizeof(g_path_bak), "%s/save.bak", g_dir);
    snprintf(g_path_csv, sizeof(g_path_csv), "%s/history.csv", g_dir);
}

const char *storage_dir(void)
{
    return g_dir;
}

/* Returns 2 for a valid v2 file, 1 for a valid v1 file, 0 if missing, -1 if damaged. */
static int read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    memset(&g_read, 0, sizeof(g_read));
    n = fread(&g_read, 1, sizeof(g_read), f);
    fclose(f);

    if (n >= sizeof(SaveBlob) && g_read.v2.magic == SAVE_MAGIC && g_read.v2.version == 2 &&
        g_read.v2.size == sizeof(SaveBlob) &&
        g_read.v2.checksum == fnv1a(&g_read.v2, offsetof(SaveBlob, checksum)))
        return 2;

    if (n == sizeof(SaveBlobV1) && g_read.v1.magic == SAVE_MAGIC && g_read.v1.version == 1 &&
        g_read.v1.size == sizeof(SaveBlobV1) &&
        g_read.v1.checksum == fnv1a(&g_read.v1, offsetof(SaveBlobV1, checksum)))
        return 1;

    return -1;
}

static void restore_timer(Pomo *p, uint32_t phase, uint32_t cycle_done, uint32_t remaining_sec)
{
    p->phase = (phase <= PH_LONG) ? phase : PH_FOCUS;
    p->cycle_done = cycle_done > p->set.long_every ? p->set.long_every : cycle_done;
    /* The timer always comes back paused, exactly where you left it. */
    p->running = 0;
    p->focus_carry_us = 0;
    p->remaining_us = (int64_t)remaining_sec * US_PER_SEC;
    if (p->remaining_us <= 0 || p->remaining_us > pomo_phase_len_us(p, p->phase))
        p->remaining_us = pomo_phase_len_us(p, p->phase);
}

static void load_v2(Pomo *p)
{
    const SaveBlob *b = &g_read.v2;
    p->set = b->set;
    pomo_clamp_settings(&p->set);
    p->total_sessions  = b->total_sessions;
    p->total_focus_sec = b->total_focus_sec;
    p->unlocked        = b->unlocked;
    memcpy(p->days, b->days, sizeof(p->days));
    restore_timer(p, b->phase, b->cycle_done, b->remaining_sec);
}

static void load_v1(Pomo *p)
{
    const SaveBlobV1 *b = &g_read.v1;
    int i;
    p->set.focus_min   = b->set.focus_min;
    p->set.short_min   = b->set.short_min;
    p->set.long_min    = b->set.long_min;
    p->set.long_every  = b->set.long_every;
    p->set.auto_start  = 0;            /* v2: phases now wait for you */
    p->set.volume      = b->set.volume;
    p->set.keep_screen = b->set.keep_screen;
    p->set.daily_goal  = 16;
    pomo_clamp_settings(&p->set);
    p->total_sessions  = b->total_sessions;
    p->total_focus_sec = b->total_focus_sec;
    for (i = 0; i < 60; i++) {
        DayRec *r;
        if (b->days[i].day < 0) continue;
        r = pomo_day(p, b->days[i].day);
        r->sessions  = (uint16_t)(b->days[i].sessions > 0xFFFF ? 0xFFFF : b->days[i].sessions);
        r->focus_sec = b->days[i].focus_sec;
        /* hours were not recorded in v1: windows use the default color */
    }
    restore_timer(p, b->phase, b->cycle_done, b->remaining_sec);
}

int storage_load(Pomo *p)
{
    int r = read_file(g_path_dat);
    int had_file = (r != 0);

    if (r < 1) {
        int r2 = read_file(g_path_bak);
        if (r2 != 0) had_file = 1;
        r = r2;
    }
    if (r == 2) { load_v2(p); return 1; }
    if (r == 1) { load_v1(p); return 2; }
    return had_file ? -1 : 0;
}

static int write_blob(const char *path)
{
    FILE *f = fopen(path, "wb");
    size_t n;
    int ok;
    if (!f) return -1;
    n = fwrite(&g_blob, 1, sizeof(g_blob), f);
    ok = (fflush(f) == 0);
    if (fclose(f) != 0) ok = 0;
    return (n == sizeof(g_blob) && ok) ? 0 : -1;
}

static void write_csv(const Pomo *p)
{
    FILE *f;
    int32_t newest = pomo_newest_day(p), d;
    if (newest < 0) return;
    f = fopen(g_path_csv, "w");
    if (!f) return;

    fprintf(f, "date,weekday,floors,focus_minutes,roof\n");
    for (d = newest - HIST_DAYS + 1; d <= newest; d++) {
        const DayRec *r = pomo_find_day(p, d);
        int y, m, dd;
        if (!r) continue;
        dt_from_day_number(d, &y, &m, &dd);
        fprintf(f, "%04d-%02d-%02d,%s,%u,%u,%s\n", y, m, dd, dt_weekday_name(d),
                (unsigned)r->sessions, (unsigned)(r->focus_sec / 60),
                r->sessions >= p->set.daily_goal ? "yes" : "no");
    }
    fprintf(f, "total,,%u,%u,\n", (unsigned)p->total_sessions,
            (unsigned)(p->total_focus_sec / 60));
    fclose(f);
}

int storage_save(const Pomo *p)
{
    int r1, r2;

    memset(&g_blob, 0, sizeof(g_blob));
    g_blob.magic   = SAVE_MAGIC;
    g_blob.version = 2;
    g_blob.size    = sizeof(g_blob);
    g_blob.set     = p->set;
    g_blob.total_sessions  = p->total_sessions;
    g_blob.total_focus_sec = p->total_focus_sec;
    g_blob.unlocked   = p->unlocked;
    g_blob.phase      = p->phase;
    g_blob.cycle_done = p->cycle_done;
    g_blob.remaining_sec = (uint32_t)((p->remaining_us + US_PER_SEC - 1) / US_PER_SEC);
    memcpy(g_blob.days, p->days, sizeof(g_blob.days));
    g_blob.checksum = fnv1a(&g_blob, offsetof(SaveBlob, checksum));

    r1 = write_blob(g_path_bak);
    r2 = write_blob(g_path_dat);
    write_csv(p);

    return (r1 == 0 || r2 == 0) ? 0 : -1;
}
