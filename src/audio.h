/*
 * audio.h - chimes synthesized in code (no sound files needed).
 * Uses pspaudiolib, the same audio library as the official PSPDEV sample.
 */
#ifndef POMO_AUDIO_H
#define POMO_AUDIO_H

enum {
    SND_NONE = 0,
    SND_CLICK,        /* short tick for button presses */
    SND_FOCUS_DONE,   /* focus finished, short break starts */
    SND_LONG_BREAK,   /* focus finished, long break starts */
    SND_BREAK_DONE    /* break finished, back to focus */
};

void audio_init(void);
void audio_shutdown(void);
void audio_set_volume(int level);   /* VOL_OFF..VOL_HIGH from core.h */
void audio_play(int sound);

#endif
