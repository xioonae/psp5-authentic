/* psp5 – made by Mille Weber */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "controls.h"
#include "sound.h"
#include "lang.h"
#include "theme.h"
#include "datetime.h"

unsigned char g_boot_anim = 1;
unsigned char g_battery_pct = 0; /* off until the player turns it on */
unsigned char g_lib_sort = 0;    /* the Library's Sort button (ui.h, LIB_SORT_*) */
unsigned short g_backlight_off = 0;
unsigned char g_lib_view = 0;    /* the Library's view button: 0 = all, 1 = only folders, 2 = only what is outside folders, 3 = both (ui.c, LIB_VIEW_*) */

void config_load(const char *data_dir)
{
    char path[200], buf[384] = "";
    snprintf(path, sizeof(path), "%s/psp5.cfg", data_dir);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        int n = sceIoRead(fd, buf, sizeof(buf) - 1);
        sceIoClose(fd);
        buf[n > 0 ? n : 0] = 0;
        const char *p;
        if ((p = strstr(buf, "sound=")))
            g_sound_on = p[6] != '0';
        if ((p = strstr(buf, "bootanim=")))
            g_boot_anim = p[9] != '0';
        if ((p = strstr(buf, "lang=")))
            g_lang_setting = lang_from_code(p + 5);
        if ((p = strstr(buf, "theme=")))
            g_theme = theme_from_code(p + 6);
        if ((p = strstr(buf, "themeart=")))
            g_theme_art = p[9] != '0';
        if ((p = strstr(buf, "batterypct=")))
            g_battery_pct = p[11] != '0';
        if ((p = strstr(buf, "librarysort="))) {
            g_lib_sort = p[12] >= '0' && p[12] < '0' + CONFIG_LIB_SORTS ? p[12] - '0' : 0;
        } else if ((p = strstr(buf, "libsort="))) { /* older files: 0 = name, 1 = recently added, 2 = recently played (not a choice any more) */
            g_lib_sort = p[8] == '1' ? 2 : 0;
        }
        if ((p = strstr(buf, "libview=")) && p[8] >= '0' && p[8] <= '3')
            g_lib_view = p[8] - '0';
        if ((p = strstr(buf, "backlightoff="))) {
            int secs = atoi(p + 13);
            g_backlight_off = secs >= BACKLIGHT_OFF_MIN && secs <= BACKLIGHT_OFF_MAX ? secs : 0;
        }
        if ((p = strstr(buf, "confirm=")))
            g_ctl_confirm = controls_from_code(p + 8);
        if ((p = strstr(buf, "back=")))
            g_ctl_back = controls_from_code(p + 5);
        if ((p = strstr(buf, "options=")))
            g_ctl_options = !strncmp(p + 8, "select", 6);
        if ((p = strstr(buf, "dst=")) && p[4] >= '0' && p[4] <= '2')
            g_dt_dst = p[4] - '0';
        if ((p = strstr(buf, "datefmt=")) && p[8] >= '0' && p[8] <= '2')
            g_dt_date = p[8] - '0';
        if ((p = strstr(buf, "timefmt=")) && p[8] >= '0' && p[8] <= '1')
            g_dt_time = p[8] - '0';
        if (g_ctl_confirm == g_ctl_back && g_ctl_confirm != CTL_PSP) /* a file with the same button twice: back goes to the PSP's */
            g_ctl_back = CTL_PSP;
    }
    lang_apply();
}

void config_save(const char *data_dir)
{
    char path[200], buf[384];
    snprintf(path, sizeof(path), "%s/psp5.cfg", data_dir);
    int n = snprintf(buf, sizeof(buf), "sound=%d\nbootanim=%d\nlang=%s\ntheme=%s\nthemeart=%d\nbatterypct=%d\nlibrarysort=%d\nlibview=%d\nconfirm=%s\nback=%s\noptions=%s\ndst=%d\ndatefmt=%d\ntimefmt=%d\n",
                     g_sound_on ? 1 : 0, g_boot_anim ? 1 : 0, lang_code(g_lang_setting), theme_cur()->code,
                     g_theme_art ? 1 : 0, g_battery_pct ? 1 : 0, g_lib_sort < CONFIG_LIB_SORTS ? g_lib_sort : 0, g_lib_view <= 3 ? g_lib_view : 0,
                     controls_code(g_ctl_confirm), controls_code(g_ctl_back), g_ctl_options ? "select" : "start",
                     g_dt_dst <= DT_DST_ADD ? g_dt_dst : DT_DST_KEEP, g_dt_date <= DT_DATE_DMY ? g_dt_date : DT_DATE_YMD, g_dt_time <= DT_TIME_12 ? g_dt_time : DT_TIME_24);
    if (g_backlight_off >= BACKLIGHT_OFF_MIN && g_backlight_off <= BACKLIGHT_OFF_MAX)
        n += snprintf(buf + n, sizeof(buf) - n, "backlightoff=%d\n", g_backlight_off);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, buf, n);
        sceIoClose(fd);
    }
}
