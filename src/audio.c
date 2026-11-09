/*
 * audio.c - bell-like chimes generated with a small additive synthesizer.
 *
 * pspaudiolib calls audio_callback() from its own thread whenever it needs
 * more samples (16-bit stereo, 44.1 kHz). The main thread only sets
 * g_request; the callback picks it up and starts the notes.
 */
#include "audio.h"
#include "core.h"

#include <pspaudiolib.h>
#include <math.h>

#define SAMPLE_RATE 44100
#define TABLE_SIZE  1024
#define MAX_VOICES  8

typedef struct {
    float freq;      /* Hz */
    int   start_ms;  /* offset from the start of the sequence */
    int   dur_ms;
    float tau;       /* decay time constant in seconds */
} Note;

typedef struct {
    int   active;
    int   delay;     /* samples to wait before sounding */
    int   left;      /* samples left to play */
    int   age;       /* samples played (for the attack ramp) */
    float phase;     /* 0..1 */
    float inc;       /* phase increment per sample */
    float env;       /* current envelope level */
    float decay;     /* per-sample envelope multiplier */
} Voice;

static float g_sine[TABLE_SIZE];
static Voice g_voices[MAX_VOICES];
static volatile int g_request = SND_NONE;
static volatile int g_volume = VOL_HIGH;
static int g_ready = 0;

/* Musical notes (Hz) */
#define C6 1046.50f
#define E6 1318.51f
#define G6 1567.98f
#define C7 2093.00f
#define G5  783.99f
#define D6 1174.66f

static const Note SEQ_CLICK[] = {
    { 1760.0f, 0, 45, 0.015f },
};

/* Focus done: bright rising triad, played twice */
static const Note SEQ_FOCUS_DONE[] = {
    { C6,   0, 700, 0.30f }, { E6, 150, 700, 0.30f }, { G6, 300, 1000, 0.40f },
    { C6, 900, 700, 0.30f }, { E6, 1050, 700, 0.30f }, { G6, 1200, 1300, 0.45f },
};

/* Long break: four-note arpeggio, played twice */
static const Note SEQ_LONG_BREAK[] = {
    { C6,   0, 700, 0.30f }, { E6, 140, 700, 0.30f }, { G6, 280, 700, 0.30f }, { C7, 420, 1300, 0.50f },
    { C6, 1100, 700, 0.30f }, { E6, 1240, 700, 0.30f }, { G6, 1380, 700, 0.30f }, { C7, 1520, 1600, 0.55f },
};

/* Break done: "ding-dong" call back to work, played twice */
static const Note SEQ_BREAK_DONE[] = {
    { G6,   0, 600, 0.35f }, { D6, 280, 900, 0.45f },
    { G6, 950, 600, 0.35f }, { D6, 1230, 1200, 0.50f },
};

/* Roof: fanfare climbing to a high C */
#define E7 2637.02f
static const Note SEQ_ROOF[] = {
    { C6,   0, 500, 0.25f }, { E6, 110, 500, 0.25f }, { G6, 220, 500, 0.25f }, { C7, 330, 900, 0.40f },
    { G6, 700, 500, 0.25f }, { C7, 810, 600, 0.30f }, { E7, 920, 1600, 0.60f }, { C6, 920, 1600, 0.60f },
};

static const float VOLUME_GAIN[4] = { 0.0f, 0.25f, 0.55f, 1.0f };

static void start_sequence(const Note *notes, int count)
{
    int i, v;
    for (i = 0; i < MAX_VOICES; i++)
        g_voices[i].active = 0;

    for (i = 0, v = 0; i < count && v < MAX_VOICES; i++, v++) {
        Voice *vo = &g_voices[v];
        const Note *n = &notes[i];
        vo->active = 1;
        vo->delay  = n->start_ms * SAMPLE_RATE / 1000;
        vo->left   = n->dur_ms * SAMPLE_RATE / 1000;
        vo->age    = 0;
        vo->phase  = 0.0f;
        vo->inc    = n->freq / (float)SAMPLE_RATE;
        vo->env    = 1.0f;
        vo->decay  = expf(-1.0f / (n->tau * (float)SAMPLE_RATE));
    }
}

static void handle_request(int req)
{
    switch (req) {
    case SND_CLICK:      start_sequence(SEQ_CLICK, 1); break;
    case SND_FOCUS_DONE: start_sequence(SEQ_FOCUS_DONE, 6); break;
    case SND_LONG_BREAK: start_sequence(SEQ_LONG_BREAK, 8); break;
    case SND_BREAK_DONE: start_sequence(SEQ_BREAK_DONE, 4); break;
    case SND_ROOF:       start_sequence(SEQ_ROOF, 8); break;
    default: break;
    }
}

static void audio_callback(void *buf, unsigned int length, void *userdata)
{
    short *out = (short *)buf;
    unsigned int i;
    int v;
    int req = g_request;
    float gain;
    int vol = g_volume;
    (void)userdata;

    if (req != SND_NONE) {
        g_request = SND_NONE;
        handle_request(req);
    }
    if (vol < 0) vol = 0;
    if (vol > 3) vol = 3;
    gain = VOLUME_GAIN[vol];

    for (i = 0; i < length; i++) {
        float s = 0.0f;
        for (v = 0; v < MAX_VOICES; v++) {
            Voice *vo = &g_voices[v];
            float ramp, base, over;
            int idx;
            if (!vo->active) continue;
            if (vo->delay > 0) { vo->delay--; continue; }

            /* 3 ms attack ramp avoids clicks */
            ramp = vo->age < 132 ? (float)vo->age / 132.0f : 1.0f;
            idx = (int)(vo->phase * TABLE_SIZE) & (TABLE_SIZE - 1);
            base = g_sine[idx];
            over = g_sine[(idx * 2) & (TABLE_SIZE - 1)];   /* octave overtone */
            s += (base + 0.35f * over) * vo->env * ramp * 0.30f;

            vo->phase += vo->inc;
            if (vo->phase >= 1.0f) vo->phase -= 1.0f;
            vo->env *= vo->decay;
            vo->age++;
            if (--vo->left <= 0) vo->active = 0;
        }

        s *= gain;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out[i * 2]     = (short)(s * 32000.0f);
        out[i * 2 + 1] = (short)(s * 32000.0f);
    }
}

void audio_init(void)
{
    int i;
    for (i = 0; i < TABLE_SIZE; i++)
        g_sine[i] = sinf(2.0f * 3.14159265f * (float)i / (float)TABLE_SIZE);
    for (i = 0; i < MAX_VOICES; i++)
        g_voices[i].active = 0;

    if (pspAudioInit() >= 0) {
        pspAudioSetChannelCallback(0, audio_callback, 0);
        g_ready = 1;
    }
}

void audio_shutdown(void)
{
    if (g_ready) {
        pspAudioEnd();
        g_ready = 0;
    }
}

void audio_set_volume(int level)
{
    g_volume = level;
}

void audio_play(int sound)
{
    if (!g_ready || g_volume == VOL_OFF) return;
    g_request = sound;
}
