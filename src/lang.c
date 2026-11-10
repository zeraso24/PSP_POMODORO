/*
 * lang.c - Spanish and English texts.
 */
#include "lang.h"
#include "datetime.h"

static int g_lang = LANG_ES;

static const char *ES[S_COUNT] = {
    [S_FOCUS] = "ENFOQUE", [S_BREAK_TIME] = "DESCANSO", [S_SHORT_BREAK] = "PAUSA CORTA",
    [S_LONG_BREAK] = "PAUSA LARGA", [S_BUILDING_FLOOR] = "PLANTA %d EN OBRA",
    [S_CRANE_LOWERS] = "La gr" L_u "a la baja mientras trabajas",
    [S_PRESS_X] = "X para empezar", [S_BONUS_FLOOR_N] = "Planta extra %d",
    [S_FLOOR_OF] = "Planta %d de %d", [S_PAUSED] = "EN PAUSA",
    [S_FLOOR_WAITING] = "La planta %d te espera", [S_ROOF_REACHED] = L_X "Techo! D" L_i "a completo",
    [S_BONUS_BUILT] = L_X "Planta extra construida!", [S_FLOOR_BUILT] = L_X "Planta %d lista! Faltan %d.",
    [S_TAKE_BREAK] = "DESCANSA", [S_CRANE_WAITS] = "La gr" L_u "a espera tu pr" L_o "xima planta",
    [S_NEW_UNLOCK_HINT] = L_X "Desbloqueo nuevo! Mira tu calle",
    [S_TODAY_LINE] = "Hoy: %d plantas - %s", [S_SAVED] = "Guardado", [S_SAVE_FAILED] = "ERROR AL GUARDAR",

    [S_H_START] = "Empezar", [S_H_PAUSE] = "Pausa", [S_H_SKIP] = "Saltar", [S_H_RESTART] = "Reiniciar",
    [S_H_CITY] = "Ciudad", [S_H_SETTINGS] = "Ajustes", [S_H_VIEW] = "Vista", [S_H_SELECT] = "Elegir",
    [S_H_CHANGE] = "Cambiar", [S_H_ACTION] = "Acci" L_o "n", [S_H_BACK] = "Volver", [S_H_DAY] = "D" L_i "a",
    [S_H_LOOK] = "Mirar", [S_H_WEEK] = "Semana", [S_H_MONTH] = "Mes", [S_H_MOVE] = "Mover",
    [S_H_TIMER] = "Reloj", [S_H_NEXT] = "Siguiente",

    [S_T_NEW_DAY] = L_X "Nuevo d" L_i "a! Edificio nuevo", [S_T_CLOCK_CHANGED] = "Reloj cambiado: fecha al d" L_i "a",
    [S_T_SAVE_CONVERTED] = "Partida actualizada, historial intacto",
    [S_T_SEASON_UP] = L_X "Paleta %s desbloqueada!", [S_T_REMIND] = "Te espero: pulsa X",

    [S_SETTINGS] = "AJUSTES", [S_SEC_TIMER] = "TEMPORIZADOR", [S_SEC_CITY] = "CIUDAD", [S_SEC_SYSTEM] = "SISTEMA",
    [S_SET_FOCUS] = "Enfoque", [S_SET_SHORT] = "Pausa corta", [S_SET_LONG] = "Pausa larga",
    [S_SET_EVERY] = "Larga tras", [S_SET_AUTO] = "Arranque auto", [S_SET_REMIND] = "Recordatorio",
    [S_SET_GOAL] = "Meta diaria", [S_SET_VIEW] = "Vista", [S_SET_LANG] = "Idioma",
    [S_SET_SOUND] = "Sonido", [S_SET_SCREEN] = "Pantalla fija", [S_SET_CLOCK] = "Hora",
    [S_SET_TEST] = "Probar sonido", [S_SET_HOWTO] = "C" L_o "mo se juega", [S_SET_CREDITS] = "Cr" L_e "ditos",
    [S_SET_RESET] = "Borrar datos",
    [S_V_MIN] = "%u min", [S_V_SESSIONS] = "%u sesiones", [S_V_FLOORS] = "%u plantas",
    [S_V_ON] = "S" L_i, [S_V_OFF] = "No", [S_V_PRESS_X] = "Pulsa X", [S_V_PRESS_AGAIN] = "X otra vez",
    [S_RESET_WARNING] = "Borra TODO tu historial y tus edificios. X otra vez.",
    [S_VOL_OFF] = "No", [S_VOL_LOW] = "Bajo", [S_VOL_MED] = "Medio", [S_VOL_HIGH] = "Alto",
    [S_VIEW_CLASSIC] = "Cl" L_a "sica", [S_VIEW_CINEMA] = "Cine", [S_VIEW_BIG] = "Reloj grande",

    [S_TAB_TODAY] = "HOY", [S_TAB_WEEK] = "SEMANA", [S_TAB_MONTH] = "MES", [S_TAB_YEAR] = "ANUAL",
    [S_FLOORS] = "PLANTAS", [S_DAY_COMPLETE] = L_X "D" L_I "A COMPLETO!", [S_BONUS_FLOORS] = "+%d planta%s extra",
    [S_CRANE_LOWERING] = "La gr" L_u "a baja la planta %d", [S_UNTIL_LANDS] = "Aterriza en %02d:%02d",
    [S_NEXT_FLOOR] = "Siguiente: planta %d", [S_START_FROM_TIMER] = "Empi" L_e "zala desde el reloj",
    [S_BREAK] = "Descanso", [S_TO_THE_ROOF] = "Faltan %d para el techo", [S_FLOORS_BUILT] = "%d plantas construidas",
    [S_NO_FLOORS] = "Sin plantas este d" L_i "a", [S_FOCUS_LABEL] = "ENFOQUE",
    [S_STREAK] = "Racha: %u d" L_i "a%s", [S_NEW_X] = L_X "Nuevo: %s!", [S_UNL_BUS] = "el autob" L_u "s",
    [S_UNL_PURPLE] = L_a "rbol morado", [S_UNL_ORANGE] = L_a "rbol naranja", [S_UNL_GREEN] = L_a "rbol verde",
    [S_STYLE] = "Estilo: %s", [S_STYLE_BROWN] = "piedra", [S_STYLE_SHOP] = "tienda", [S_STYLE_CLUB] = "club",
    [S_WEEK_LINE] = "%u planta%s - %u techo%s - %s",
    [S_MONTH_LINE] = "%u planta%s - %u techo%s - media %.1f - mejor %d (d" L_i "a %d)",
    [S_SEASON_LINE] = "Paleta %s - %u/%u para %s", [S_SEASON_MAX] = "Paleta %s - " L_X "nivel m" L_a "ximo!",
    [S_YEAR_FLOORS] = "plantas", [S_YEAR_ROOFS] = "%u techo%s", [S_BEST_STREAK] = "Mejor racha: %u d" L_i "as",
    [S_NOT_YET] = "A" L_u "n no ha llegado", [S_DAY_FLOORS] = "%d plantas%s", [S_DAY_ROOF] = " - " L_X "techo!",
    [S_DAY_FOCUS] = "%s de enfoque", [S_LEG_LOW] = "poco", [S_LEG_HIGH] = "mucho", [S_LEG_ROOF] = "techo",
    [S_SEASON_0] = "CL" L_A "SICA", [S_SEASON_1] = "ATARDECER", [S_SEASON_2] = "MENTA", [S_SEASON_3] = "ORO",
    [S_SEASON_4] = "NE" L_O "N",

    [S_HUD_FLOOR] = "PLANTA %d/%d", [S_HUD_STREAK] = "Racha %u d" L_i "as",
    [S_BIG_STATE] = "%s - PLANTA %d", [S_BIG_SAVER] = "Modo ahorro",

    [S_TUT_TITLE] = "C" L_O "MO FUNCIONA %d/3",
    [S_TUT_1A] = "Cada sesi" L_o "n de enfoque construye", [S_TUT_1B] = "una planta del edificio de hoy.",
    [S_TUT_2A] = "Mientras te concentras, la gr" L_u "a", [S_TUT_2B] = "baja la planta. Al acabar aterriza:",
    [S_TUT_2C] = "pulsa X para descansar.",
    [S_TUT_3A] = "Llega a tu meta diaria y la gr" L_u "a", [S_TUT_3B] = "pone el techo. " L_X "Fuegos artificiales!",
    [S_TUT_3C] = "L/R: vista   SELECT: tu ciudad",
    [S_TUT_FOOTER] = "X Siguiente     O Saltar",

    [S_CREDITS] = "CR" L_E "DITOS", [S_CR_VERSION] = "Versi" L_o "n 3",
    [S_CR_BUILDINGS] = "Edificios, calle y cielo de atardecer:",
    [S_CR_RESIDENTS] = "Vecinos y cielo de d" L_i "a:",
    [S_CR_TOOLCHAIN] = "Hecho con las herramientas PSPDEV",
};

static const char *EN[S_COUNT] = {
    [S_FOCUS] = "FOCUS", [S_BREAK_TIME] = "BREAK TIME", [S_SHORT_BREAK] = "SHORT BREAK",
    [S_LONG_BREAK] = "LONG BREAK", [S_BUILDING_FLOOR] = "BUILDING FLOOR %d",
    [S_CRANE_LOWERS] = "The crane lowers it as you focus",
    [S_PRESS_X] = "Press X to start", [S_BONUS_FLOOR_N] = "Bonus floor %d",
    [S_FLOOR_OF] = "Floor %d of %d", [S_PAUSED] = "PAUSED",
    [S_FLOOR_WAITING] = "Floor %d is waiting for you", [S_ROOF_REACHED] = "Roof reached! Day complete!",
    [S_BONUS_BUILT] = "Bonus floor built!", [S_FLOOR_BUILT] = "Floor %d built! %d to the roof.",
    [S_TAKE_BREAK] = "TAKE A BREAK", [S_CRANE_WAITS] = "The crane waits for your next floor",
    [S_NEW_UNLOCK_HINT] = "New unlock! See your street",
    [S_TODAY_LINE] = "Today: %d floors - %s", [S_SAVED] = "Saved", [S_SAVE_FAILED] = "SAVE FAILED",

    [S_H_START] = "Start", [S_H_PAUSE] = "Pause", [S_H_SKIP] = "Skip", [S_H_RESTART] = "Restart",
    [S_H_CITY] = "City", [S_H_SETTINGS] = "Settings", [S_H_VIEW] = "View", [S_H_SELECT] = "Select",
    [S_H_CHANGE] = "Change", [S_H_ACTION] = "Action", [S_H_BACK] = "Back", [S_H_DAY] = "Day",
    [S_H_LOOK] = "Look", [S_H_WEEK] = "Week", [S_H_MONTH] = "Month", [S_H_MOVE] = "Move",
    [S_H_TIMER] = "Timer", [S_H_NEXT] = "Next",

    [S_T_NEW_DAY] = "New day! New building, floor 0", [S_T_CLOCK_CHANGED] = "Clock changed - date updated",
    [S_T_SAVE_CONVERTED] = "Save updated - history kept",
    [S_T_SEASON_UP] = "%s palette unlocked!", [S_T_REMIND] = "Waiting for you: press X",

    [S_SETTINGS] = "SETTINGS", [S_SEC_TIMER] = "TIMER", [S_SEC_CITY] = "CITY", [S_SEC_SYSTEM] = "SYSTEM",
    [S_SET_FOCUS] = "Focus", [S_SET_SHORT] = "Short break", [S_SET_LONG] = "Long break",
    [S_SET_EVERY] = "Long after", [S_SET_AUTO] = "Auto-start", [S_SET_REMIND] = "Reminder",
    [S_SET_GOAL] = "Daily goal", [S_SET_VIEW] = "Clock view", [S_SET_LANG] = "Language",
    [S_SET_SOUND] = "Sound", [S_SET_SCREEN] = "Keep screen", [S_SET_CLOCK] = "Clock fix",
    [S_SET_TEST] = "Test sound", [S_SET_HOWTO] = "How to play", [S_SET_CREDITS] = "Credits",
    [S_SET_RESET] = "Reset stats",
    [S_V_MIN] = "%u min", [S_V_SESSIONS] = "%u sessions", [S_V_FLOORS] = "%u floors",
    [S_V_ON] = "On", [S_V_OFF] = "Off", [S_V_PRESS_X] = "Press X", [S_V_PRESS_AGAIN] = "Press X again",
    [S_RESET_WARNING] = "Erases ALL your history and buildings. X again to confirm.",
    [S_VOL_OFF] = "Off", [S_VOL_LOW] = "Low", [S_VOL_MED] = "Medium", [S_VOL_HIGH] = "High",
    [S_VIEW_CLASSIC] = "Classic", [S_VIEW_CINEMA] = "Cinema", [S_VIEW_BIG] = "Big clock",

    [S_TAB_TODAY] = "TODAY", [S_TAB_WEEK] = "WEEK", [S_TAB_MONTH] = "MONTH", [S_TAB_YEAR] = "YEAR",
    [S_FLOORS] = "FLOORS", [S_DAY_COMPLETE] = "DAY COMPLETE!", [S_BONUS_FLOORS] = "+%d bonus floor%s",
    [S_CRANE_LOWERING] = "Crane lowering floor %d", [S_UNTIL_LANDS] = "%02d:%02d until it lands",
    [S_NEXT_FLOOR] = "Next: floor %d", [S_START_FROM_TIMER] = "Start it from the timer",
    [S_BREAK] = "Break time", [S_TO_THE_ROOF] = "%d floors to the roof", [S_FLOORS_BUILT] = "%d floors built",
    [S_NO_FLOORS] = "No floors this day", [S_FOCUS_LABEL] = "FOCUS",
    [S_STREAK] = "Streak: %u day%s", [S_NEW_X] = "New: %s unlocked!", [S_UNL_BUS] = "the city bus",
    [S_UNL_PURPLE] = "purple tree", [S_UNL_ORANGE] = "orange tree", [S_UNL_GREEN] = "green tree",
    [S_STYLE] = "Style: %s", [S_STYLE_BROWN] = "brownstone", [S_STYLE_SHOP] = "shop", [S_STYLE_CLUB] = "club",
    [S_WEEK_LINE] = "%u floor%s - %u roof%s - %s focus",
    [S_MONTH_LINE] = "%u floor%s - %u roof%s - avg %.1f/day - best %d (day %d)",
    [S_SEASON_LINE] = "%s palette - %u/%u for %s", [S_SEASON_MAX] = "%s palette - top level!",
    [S_YEAR_FLOORS] = "floors", [S_YEAR_ROOFS] = "%u roof%s", [S_BEST_STREAK] = "Best streak: %u days",
    [S_NOT_YET] = "Not here yet", [S_DAY_FLOORS] = "%d floors%s", [S_DAY_ROOF] = " - roof!",
    [S_DAY_FOCUS] = "%s focus", [S_LEG_LOW] = "low", [S_LEG_HIGH] = "high", [S_LEG_ROOF] = "roof",
    [S_SEASON_0] = "CLASSIC", [S_SEASON_1] = "SUNSET", [S_SEASON_2] = "MINT", [S_SEASON_3] = "GOLD",
    [S_SEASON_4] = "NEON",

    [S_HUD_FLOOR] = "FLOOR %d/%d", [S_HUD_STREAK] = "Streak %u days",
    [S_BIG_STATE] = "%s - FLOOR %d", [S_BIG_SAVER] = "Battery saver",

    [S_TUT_TITLE] = "HOW IT WORKS %d/3",
    [S_TUT_1A] = "Every focus session builds", [S_TUT_1B] = "one floor of today's building.",
    [S_TUT_2A] = "While you focus, the crane", [S_TUT_2B] = "lowers the floor. When time is up",
    [S_TUT_2C] = "it lands: press X for your break.",
    [S_TUT_3A] = "Reach your daily goal and the crane", [S_TUT_3B] = "sets the roof. Fireworks!",
    [S_TUT_3C] = "L/R: clock view   SELECT: your city",
    [S_TUT_FOOTER] = "X Next     O Skip",

    [S_CREDITS] = "CREDITS", [S_CR_VERSION] = "Version 3",
    [S_CR_BUILDINGS] = "Buildings, street and dusk sky:",
    [S_CR_RESIDENTS] = "Residents and day sky:",
    [S_CR_TOOLCHAIN] = "Built with the PSPDEV toolchain",
};

void lang_set(int lang) { g_lang = (lang == LANG_EN) ? LANG_EN : LANG_ES; }
int  lang_get(void) { return g_lang; }

const char *T(StrId id)
{
    const char *s;
    if (id < 0 || id >= S_COUNT) return "";
    s = (g_lang == LANG_EN) ? EN[id] : ES[id];
    if (!s) s = EN[id];
    return s ? s : "";
}

const char *lang_weekday(int32_t day)
{
    static const char *es[7] = { "Lun", "Mar", "Mi" L_e, "Jue", "Vie", "S" L_a "b", "Dom" };
    static const char *en[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    int w = dt_weekday(day);
    return g_lang == LANG_EN ? en[w] : es[w];
}

const char *lang_month_short(int m)
{
    static const char *es[12] = { "ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC" };
    static const char *en[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    if (m < 1 || m > 12) return "???";
    return g_lang == LANG_EN ? en[m - 1] : es[m - 1];
}

const char *lang_month_long(int m)
{
    static const char *es[12] = { "ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO", "JULIO",
                                  "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE" };
    static const char *en[12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY",
                                  "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
    if (m < 1 || m > 12) return "???";
    return g_lang == LANG_EN ? en[m - 1] : es[m - 1];
}

const char *lang_language_name(int lang)
{
    return lang == LANG_EN ? "English" : "Castellano";
}
