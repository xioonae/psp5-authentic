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
 *   libsort=0        (the Library's Sort button: 0 = name, 1 = recently added)
 *   libview=0        (the Library's view button: 0 = all games and apps, 1 = only folders, 2 = only what is outside folders, 3 = both) */
extern unsigned char g_boot_anim;
extern unsigned char g_battery_pct;
extern unsigned char g_lib_sort;
extern unsigned char g_lib_view;

void config_load(const char *data_dir);
void config_save(const char *data_dir);

#endif
