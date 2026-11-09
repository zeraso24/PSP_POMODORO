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

#define SAVE_MAGIC   0x52444D50u  /* "PMDR" */
#define SAVE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    Settings set;
    uint32_t total_sessions;
    uint32_t total_focus_sec;
    uint32_t phase;
    uint32_t cycle_done;
    uint32_t remaining_sec;
    uint32_t reserved[7];
    DayRec   days[HIST_DAYS];
    uint32_t checksum;           /* FNV-1a of everything above */
} SaveBlob;

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

static int read_blob(const char *path, SaveBlob *out)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(out, 1, sizeof(*out), f);
    fclose(f);

    if (n != sizeof(*out)) return -1;
    if (out->magic != SAVE_MAGIC || out->version != SAVE_VERSION || out->size != sizeof(*out))
        return -1;
    if (out->checksum != fnv1a(out, offsetof(SaveBlob, checksum)))
        return -1;
    return 1;
}

static int write_blob(const char *path, const SaveBlob *b)
{
    FILE *f = fopen(path, "wb");
    size_t n;
    int ok;
    if (!f) return -1;
    n = fwrite(b, 1, sizeof(*b), f);
    ok = (fflush(f) == 0);
    if (fclose(f) != 0) ok = 0;
    return (n == sizeof(*b) && ok) ? 0 : -1;
}

int storage_load(Pomo *p)
{
    SaveBlob b;
    int r = read_blob(g_path_dat, &b);
    int had_file = (r != 0);

    if (r != 1) {
        int r2 = read_blob(g_path_bak, &b);
        if (r2 != 0) had_file = 1;
        r = r2;
    }
    if (r != 1)
        return had_file ? -1 : 0;

    p->set = b.set;
    pomo_clamp_settings(&p->set);
    p->total_sessions  = b.total_sessions;
    p->total_focus_sec = b.total_focus_sec;
    memcpy(p->days, b.days, sizeof(p->days));

    p->phase = (b.phase <= PH_LONG) ? b.phase : PH_FOCUS;
    p->cycle_done = b.cycle_done;
    if (p->cycle_done > p->set.long_every) p->cycle_done = p->set.long_every;

    /* The timer always comes back paused, exactly where you left it. */
    p->running = 0;
    p->focus_carry_us = 0;
    p->remaining_us = (int64_t)b.remaining_sec * US_PER_SEC;
    if (p->remaining_us <= 0 || p->remaining_us > pomo_phase_len_us(p, p->phase))
        p->remaining_us = pomo_phase_len_us(p, p->phase);

    return 1;
}

static void write_csv(const Pomo *p)
{
    FILE *f = fopen(g_path_csv, "w");
    int i;
    if (!f) return;

    fprintf(f, "date,weekday,sessions,focus_minutes\n");
    /* days[] is newest first: write oldest first */
    for (i = HIST_DAYS - 1; i >= 0; i--) {
        const DayRec *r = &p->days[i];
        int y, m, d;
        if (r->day < 0) continue;
        dt_from_day_number(r->day, &y, &m, &d);
        fprintf(f, "%04d-%02d-%02d,%s,%u,%u\n", y, m, d, dt_weekday_name(r->day),
                (unsigned)r->sessions, (unsigned)(r->focus_sec / 60));
    }
    fprintf(f, "total,,%u,%u\n", (unsigned)p->total_sessions,
            (unsigned)(p->total_focus_sec / 60));
    fclose(f);
}

int storage_save(const Pomo *p)
{
    SaveBlob b;
    int r1, r2;

    memset(&b, 0, sizeof(b));
    b.magic   = SAVE_MAGIC;
    b.version = SAVE_VERSION;
    b.size    = sizeof(b);
    b.set     = p->set;
    b.total_sessions  = p->total_sessions;
    b.total_focus_sec = p->total_focus_sec;
    b.phase      = p->phase;
    b.cycle_done = p->cycle_done;
    b.remaining_sec = (uint32_t)((p->remaining_us + US_PER_SEC - 1) / US_PER_SEC);
    memcpy(b.days, p->days, sizeof(b.days));
    b.checksum = fnv1a(&b, offsetof(SaveBlob, checksum));

    r1 = write_blob(g_path_bak, &b);
    r2 = write_blob(g_path_dat, &b);
    write_csv(p);

    return (r1 == 0 || r2 == 0) ? 0 : -1;
}
