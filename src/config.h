/* psp5 – made by Mille Weber */
#ifndef CONFIG_H
#define CONFIG_H

/* psp5:s egna inställningar (ljud, språk, tema) i <data_dir>/psp5.cfg:
 *   sound=1
 *   bootanim=1       (startskärmen)
 *   lang=auto        (auto, en, sv, de, fr, es, it, nl, pt, ru, ja)
 *   theme=ps5        (se theme.c)
 *   themeart=1       (spelens bilder bakom korten)
 *   batterypct=0     (the battery level as a number, e.g. "87%", between the clock and the battery)
 *   libsort=0        (the Library's Sort button: 0 = name, 1 = recently added, 2 = recently played)
 *   libview=0        (the Library's view button: 0 = all games and apps, 1 = only folders, 2 = only what is outside folders, 3 = both)
 *   confirm=psp      (the button that confirms: psp = the PSP's own setting, or cross, circle, square, triangle; controls.h)
 *   back=psp         (the button that goes back: the same words)
 *   options=start    (start = START opens the options and SELECT the power menu, select = the other way round)
 *   backlightoff=120  (seconds: the backlight auto-off time chosen in Settings -> Display and Sound; psp5 sets it again at every start, because
 *                      the PSP forgets it at a restart. The line is not there until a time has been chosen) */
extern unsigned char g_boot_anim;
extern unsigned char g_battery_pct;
extern unsigned char g_lib_sort;
extern unsigned char g_lib_view;
extern unsigned short g_backlight_off; /* 0 = no time chosen in psp5 (the PSP's own stays), else seconds, BACKLIGHT_OFF_MIN .. BACKLIGHT_OFF_MAX */
#define BACKLIGHT_OFF_MIN 10
#define BACKLIGHT_OFF_MAX 3600

void config_load(const char *data_dir);
void config_save(const char *data_dir);

#endif
