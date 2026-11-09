# Pomodoro for PSP

A focus timer for the PSP that tracks your sessions and saves your history to the Memory Stick. It runs as a normal homebrew app on custom firmware such as ARK-4.

Built on the official [PSPDEV](https://pspdev.github.io) template: CMake + `create_pbp_file`, `pspaudiolib` for sound.

## Features

- Focus / short break / long break cycle (25 / 5 / 15 min by default, long break after 4 sessions).
- A chime at the end of every phase, with a different sound for each: focus done, long break, back to work.
- Tracks completed sessions and focused minutes per day, last 7 days, all time, and your daily streak.
- Saves to the Memory Stick automatically, with a backup copy in case a save gets interrupted.
- Exports `history.csv` so you can open your history on a computer.
- Uses the real-time clock, so the timer stays accurate even if the PSP goes to sleep. When it wakes, a finished phase is completed and the chime plays.
- Keeps the PSP from auto-sleeping while the timer runs; optionally keeps the backlight on.
- Shows the clock and battery level.

## Controls

| Screen | Button | Action |
| --- | --- | --- |
| Timer | ✕ | Start / pause |
| Timer | □ | Skip to the next phase (not counted as a completed session) |
| Timer | △ | Restart the current phase |
| Timer | SELECT | Statistics |
| Timer | START | Settings |
| Statistics | ○ | Back |
| Settings | ↑ ↓ | Choose a setting |
| Settings | ← → | Change it (hold to change faster) |
| Settings | ✕ | Test sound / reset statistics (press twice) |
| Settings | ○ or START | Save and go back |
| Anywhere | HOME | Quit (everything is saved; the timer comes back paused) |

## Where your data is saved

In the app's own folder on the Memory Stick, next to `EBOOT.PBP`:

```
ms0:/PSP/GAME/Pomodoro/
├── EBOOT.PBP
├── save.dat       ← settings, history, paused timer
├── save.bak       ← backup copy, used automatically if save.dat is damaged
└── history.csv    ← your daily history (date, weekday, sessions, focus minutes)
```

It saves when a phase ends, when you pause, skip or restart, when you leave Settings, every minute while the timer runs, and when you quit. The timer screen shows "Saved" or "SAVE FAILED" in the bottom right.

When you update the app, replace only `EBOOT.PBP`, so your history stays.

## Build option A: GitHub (nothing to install)

1. Create a new repository on GitHub and upload this whole folder, including the hidden `.github` folder.
2. Open the **Actions** tab. The "Build EBOOT" workflow runs automatically on every push; you can also start it by hand with **Run workflow**.
3. When it finishes (green check), open the run and download the **Pomodoro-PSP** artifact. It contains `PSP/GAME/Pomodoro/EBOOT.PBP`.

The workflow uses the official `pspdev/pspdev` Docker image, so it always builds with the current SDK.

## Build option B: on your Mac

Install the SDK following the [official macOS guide](https://pspdev.github.io/installation/macos.html) (Homebrew dependencies, download the SDK for Apple Silicon or Intel, set `PSPDEV` in `~/.zprofile`, remove the quarantine flag). Then:

```sh
cd Pomodoro
mkdir build && cd build
psp-cmake ..
make
```

`EBOOT.PBP` appears in the `build` folder.

## Install on your PSP

1. Connect the PSP over USB (Settings → USB Connection) or put the Memory Stick in a card reader.
2. Create the folder `PSP/GAME/Pomodoro/` on the Memory Stick.
3. Copy `EBOOT.PBP` into it.
4. On the PSP, go to Game → Memory Stick and launch **Pomodoro** (with ARK-4 running).

## Test in PPSSPP first (optional)

Put the `Pomodoro` folder with `EBOOT.PBP` inside PPSSPP's own `PSP/GAME` folder (in PPSSPP: Settings → System → Memory Stick folder) and launch it from PPSSPP's game list. Launching it from another folder works too, but saving may fail there, because the app saves next to its EBOOT.

## Good to know

- **Sleep mode:** if you put the PSP to sleep with the power switch, nothing plays while it sleeps. When you wake it, the timer catches up immediately and plays the chime if the phase ended.
- **No sound?** Check the PSP volume buttons and the mute/sound button, and that Sound isn't set to Off in Settings.
- **Keep screen:** Off (default) lets the backlight dim per the PSP's power settings to save battery, while still preventing auto-sleep. On keeps the backlight on while the timer runs.
- **Changing lengths:** a new focus or break length applies immediately if the current phase hasn't started; otherwise it applies from the next phase.

## Project layout

```
CMakeLists.txt              build file (official PSPDEV template)
.github/workflows/build.yml cloud build with the pspdev Docker image
assets/ICON0.PNG            XMB icon (144x80)
assets/PIC1.PNG             XMB background (480x272)
src/main.c                  main loop, screens, input, saving, power
src/core.c / core.h         timer state machine and statistics (no PSP code)
src/storage.c / storage.h   save files with checksum + backup, CSV export
src/gfx.c / gfx.h           double-buffered drawing and scalable pixel text
src/audio.c / audio.h       chimes synthesized in code (no sound files)
src/datetime.c / datetime.h calendar helpers
```
