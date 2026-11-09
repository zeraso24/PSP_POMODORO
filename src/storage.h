/*
 * storage.h - saves settings, statistics and the paused timer to the
 * Memory Stick, in the app's own folder (next to EBOOT.PBP):
 *
 *   save.dat     main save file
 *   save.bak     backup copy (used if save.dat is damaged)
 *   history.csv  your daily history, readable on a computer
 *
 * Uses standard C file functions, which the PSPSDK maps to the Memory Stick.
 */
#ifndef POMO_STORAGE_H
#define POMO_STORAGE_H

#include "core.h"

/* Pass argv[0] (the path of EBOOT.PBP). NULL uses ms0:/PSP/GAME/Pomodoro */
void        storage_init(const char *argv0);
const char *storage_dir(void);

/* Returns 1 if a save was loaded, 0 if none existed, -1 if all copies were damaged. */
int         storage_load(Pomo *p);

/* Returns 0 on success, -1 on failure. */
int         storage_save(const Pomo *p);

#endif
