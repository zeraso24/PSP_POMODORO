# Pomodoro City for PSP

A focus timer for the PSP where every completed focus session builds one floor of the day's building. Reach your daily goal (16 floors by default) and the crane sets the roof on top, with fireworks. Your weeks become streets, your months skylines and your year a whole city.

Runs as a normal homebrew app on custom firmware such as ARK-4. Built on the official [PSPDEV](https://pspdev.github.io) template.

## What's new in version 3

- **Spanish interface** (default) or English: Settings → Idioma / Language.
- **Three timer views**, switched with **L / R** on the timer and remembered:
  - **Classic:** the digits on the left, the building site on the right.
  - **Cinema:** the building site full screen, with the clock on a retro scoreboard that dims after 5 seconds without touching anything.
  - **Big clock:** huge digits and a mini tower; refreshes once a second at a low CPU speed to save battery.
- **Monthly seasons:** floors built in a month unlock building palettes for that month: Sunset (40), Mint (100), Gold (180) and Neon (260). A fanfare announces each one and the month's buildings repaint. The Month view shows how many floors you need for the next palette, and past months keep their colors.
- **Residents:** people from the City Mega Pack lean out of the windows.
- **Day sky:** daytime uses a blue sky with clouds and a city in the distance; dusk keeps the pink sky; night is dark.
- **Reminder** (Off / 3 / 5 / 10 min): a soft chime if a finished phase keeps waiting for ✕ (up to 5 times).
- **How-to** on first start (also in Settings → Cómo se juega).
- **Settings grouped** into Timer, City and System.

## What's new in version 2.1

- **The day changes at midnight, by the PSP's own clock.** The app now reads the local date and time directly from the PSP (the same clock the XMB shows). At midnight a new building starts at floor 0, and a session that crosses midnight counts for the new day.
- **Clock fix** (Settings): if your PSP's clock is off, correct it in half-hour steps (±12 h). You'll see the corrected time right next to it.
- The timer header now shows the weekday and date the app is using, so you can check it at a glance.

## What's new in version 2

- **Every phase waits for you.** When a session or break ends, the chime plays and the timer waits until you press ✕. (You can still turn Auto-start on in Settings.)
- **Build a city.** Each focus session is a floor. While you focus, the crane lowers the floor toward the building; it lands when time is up.
- **The roof.** On the floor that reaches your daily goal, the crane brings the cornice and flag, fireworks go off and the camera travels down the whole tower.
- **Windows show when you worked:** yellow in the morning, orange in the afternoon, blue in the evening.
- **Three building styles** (brownstone, shop, club) rotate day by day.
- **Unlocks for your street:** green tree (3-day streak), orange tree (first roof), purple tree (7-day streak), the city bus (5 roofs).
- **3 years of history**, with the hour of every session.
- **Your version-1 save is converted automatically.** Your history is kept, and Auto-start is switched off.

## Controls

| Screen | Button | Action |
| --- | --- | --- |
| Timer | ✕ | Start / pause |
| Timer | □ | Skip to the next phase (no floor is built) |
| Timer | △ | Restart the current phase |
| Timer | SELECT | Open the city |
| Timer | START | Settings |
| City | L / R | Switch view: Today, Week, Month, Year |
| City | ← → | Previous / next day, week or month (Year: move the cursor) |
| Today | ↑ ↓ | Look up and down the tower (✕ follows the top again) |
| Year | ↑ ↓ | Move the cursor a week |
| City | ○ or SELECT | Back to the timer |
| Celebration | any button | Skip the show |
| Settings | ↑ ↓ / ← → / ✕ | Choose / change / action |
| Anywhere | HOME | Quit (everything is saved; the timer comes back paused) |

Settings: focus, short and long break lengths, long break after N sessions, **daily goal** (4–24 floors), auto-start, sound, keep screen on, **clock fix**, test sound, credits, reset statistics (press ✕ twice).

The app uses the PSP's date and time (System Settings → Date & Time Settings and Time Zone). If they're wrong there, the app is wrong too: fix them on the PSP, or use Clock fix.

## Where your data is saved

In the app's folder on the Memory Stick, next to `EBOOT.PBP`:

```
ms0:/PSP/GAME/Pomodoro/
├── EBOOT.PBP
├── save.dat       ← settings, 3 years of history, paused timer
├── save.bak       ← backup copy, used automatically if save.dat is damaged
└── history.csv    ← date, weekday, floors, focus minutes, roof (open it on a computer)
```

To update from version 1, replace only `EBOOT.PBP`. Keep `save.dat` and `save.bak`, and the app converts them on first start.

## Build option A: GitHub (nothing to install)

1. Create a new repository on GitHub and upload this whole folder, including the hidden `.github` folder.
2. Open the **Actions** tab. The "Build EBOOT" workflow runs on every push; you can also start it with **Run workflow**.
3. When it finishes, download the **Pomodoro-PSP** artifact. It contains `PSP/GAME/Pomodoro/EBOOT.PBP`.

## Build option B: on your Mac

Install the SDK following the [official macOS guide](https://pspdev.github.io/installation/macos.html), then:

```sh
cd Pomodoro
mkdir build && cd build
psp-cmake ..
make
```

## Install on your PSP

1. Connect the PSP over USB or put the Memory Stick in a card reader.
2. Copy `EBOOT.PBP` into `PSP/GAME/Pomodoro/` (create the folder if needed).
3. On the PSP, go to Game → Memory Stick and launch **Pomodoro City**.

## Good to know

- **Battery:** the city views animate at 30 fps and raise the CPU to 333 MHz while open; the timer screen animates at 15 fps at the normal 222 MHz. Go back to the timer when you're not looking at the city.
- **Sleep mode:** if the PSP sleeps, the timer catches up when it wakes, and the chime plays if the phase ended.
- **Sky:** it follows your clock: the pack's painted sky at dusk, a computed sky at other hours.
- **Keep screen:** Off lets the backlight dim to save battery while still preventing auto-sleep.

## Art and credits

Residents and the day sky: **"City Mega Pack" by GrafxKid**, CC0, from <https://opengameart.org/content/city-mega-pack> (in `assets/city-mega-pack/`).

Buildings, trees, bus, signs, city silhouettes and dusk sky: **"2D Pixel City Pack" by laavis**, licensed [CC-BY 4.0](https://creativecommons.org/licenses/by/4.0/), from <https://opengameart.org/content/2d-pixel-city-pack>. The original sheets are in `assets/pixel-city-pack/`. The credits are also shown in the app (Settings → Credits).

The buildings are cut into a ground floor, a repeatable floor and a cornice so they can be stacked. `tools/gen_assets.py` converts the sheets into `src/assets.c` (needs Python with Pillow, NumPy and SciPy):

```sh
python3 tools/gen_assets.py assets/pixel-city-pack/10102017.png assets/pixel-city-pack/Spritesheet.png src assets/city-mega-pack/CITY_MEGA.png
```

## Project layout

```
CMakeLists.txt              build file (official PSPDEV template)
.github/workflows/build.yml cloud build with the pspdev Docker image
assets/ICON0.PNG, PIC1.PNG  XMB icon and background
assets/pixel-city-pack/     the art pack (CC-BY 4.0) and its credits
tools/gen_assets.py         converts the art pack into C data
src/main.c                  main loop, timer screen, settings, credits, saving, power
src/views.c                 the city: Today, Week, Month, Year and the timer's window
src/city.c / city.h         towers, cranes, street life, fireworks, isometric blocks
src/assets.c / assets.h     the converted sprites (generated)
src/core.c / core.h         timer, floors, statistics, unlocks (no PSP code)
src/storage.c / storage.h   save files with checksum + backup, v1 conversion, CSV
src/gfx.c / gfx.h           drawing: RAM back buffer, sprites, blending, pixel text
src/audio.c / audio.h       chimes and the roof fanfare, synthesized in code
src/app.h                   state shared between the screens
src/datetime.c / .h         calendar helpers
```
