/* psp5 – made by Mille Weber */
/* Systemfunktioner som arkMenu har: USB, ström, XMB och återställningsmeny.
 * Fungerar på både ARK-4 och ARK-5 – versionen känns av vid start. */
#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspimpose.h>
#include <psputility.h>
#include <psploadexec_kernel.h>
#include <psppower.h>
#include <pspwlan.h>
#include <pspreg.h>
#include <kubridge.h>
#include <systemctrl.h>
#include <systemctrl_ark.h>
#include <systemctrl_se.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "system.h"
#include "launch.h"
#include "arkcfg.h"

#define USBSTOR_EF_DRIVERNAME "USBStorEFlash_Driver"
/* finns när launcherläget är pausat för att psp5 gick till XMB */
static const char *paused_mark(void)
{
    static char path[32];
    snprintf(path, sizeof(path), "%s/launcher.paused", g_data_dir);
    return path;
}

int scePowerRequestSuspend(void);
int scePowerRequestColdReset(int unk);
int scePowerRequestStandby(void);

static int exists(const char *path);

/* ARK-4: sctrlArkGetConfig ligger i SystemCtrlForUser (src/ark4stub.S) */
ARKConfig *sctrlArkGetConfig_ark4(ARKConfig *conf);

static ARKConfig s_conf;
static int s_ark;        /* 4, 5 eller 0 = ingen ARK */
static int s_model = -1; /* kuKernelGetModel(), -1 = okänd */
static int s_has_ark;
static int s_is_launcher;
static int s_usb_on;
static char s_ark_dir[ARK_PATH_SIZE];
static int s_ms_is_ef; /* ARK låter ms0: peka på PSP Go:s internminne */

char g_root[5] = "ms0:";
char g_data_dir[16] = "ms0:/PSP5";

void sys_set_root(const char *self_path)
{
    /* som app i ef0:/PSP/GAME/PSP5 eller som launcher i ef0:/PSP/SAVEDATA/ARK_01234 */
    if (self_path && !strncasecmp(self_path, "ef0:", 4))
        snprintf(g_root, sizeof(g_root), "ef0:");
    snprintf(g_data_dir, sizeof(g_data_dir), "%s/PSP5", g_root);
}

#ifdef PSP5_TEST
/* Testbygget i PPSSPP (som saknar ARK): <data>/fake.txt med t.ex. "model=4 ark=4" låtsas vara en
 * PSP Go med ARK-4, så att inställningarna för varje modell och ARK går att se. */
static void fake_device(void)
{
    char path[64], buf[64] = "";
    snprintf(path, sizeof(path), "%s/fake.txt", g_data_dir);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return;
    int n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    buf[n > 0 ? n : 0] = 0;
    const char *p;
    if ((p = strstr(buf, "model=")))
        s_model = atoi(p + 6);
    if ((p = strstr(buf, "ark=")))
        s_ark = atoi(p + 4);
}
#endif

void sys_init(const char *self_path)
{
    char path[200];
    snprintf(s_ark_dir, sizeof(s_ark_dir), "%s/PSP/SAVEDATA/ARK_01234/", g_root);
    /* ARK:s funktioner finns bara med CFW – inte i t.ex. PPSSPP */
    if (launch_supported()) {
        int major = sctrlSEGetVersion();
        s_ark = major == 4 ? 4 : 5; /* ARK-4 ger 4, ARK-5 ger 5 */
        s_model = kuKernelGetModel();
        s_ms_is_ef = sctrlKernelMsIsEf() == 1;
        memset(&s_conf, 0, sizeof(s_conf));
        /* anropa bara den variant som finns i den ARK som körs */
        if (s_ark == 4)
            sctrlArkGetConfig_ark4(&s_conf);
#ifndef PSP5_ARK4
        else
            sctrlArkGetConfig(&s_conf);
#endif
        char settings[200], vboot[200];
        snprintf(settings, sizeof(settings), "%sSETTINGS.TXT", s_ark_dir);
        snprintf(vboot, sizeof(vboot), "%sVBOOT.PBP", s_ark_dir);
        snprintf(path, sizeof(path), "%s/SEPLUGINS/SETTINGS.TXT", g_root);
        if (IS_ARK_CONFIG(&s_conf) && s_conf.arkpath[0]) {
            s_has_ark = 1;
            snprintf(s_ark_dir, sizeof(s_ark_dir), "%s", s_conf.arkpath);
        } else if (exists(settings) || exists(vboot)) {
            s_has_ark = 1; /* konfigurationen gick inte att läsa – standardmappen finns */
        } else if (s_ark == 4 && exists(path)) {
            s_has_ark = 1; /* ARK-4 utan ARK-mapp använder <lagring>/SEPLUGINS/ */
            snprintf(s_ark_dir, sizeof(s_ark_dir), "%s/SEPLUGINS/", g_root);
        }
    }
#ifdef PSP5_TEST
    fake_device();
#endif
    g_arkcfg_ark4 = s_ark == 4;
    snprintf(path, sizeof(path), "%sVBOOT.PBP", s_ark_dir);
    s_is_launcher = self_path && !strcasecmp(self_path, path);
}

/* The PSP's own button assignment (the XMB's Settings > Button Assignment): which of ○ and ✕ confirms. The system parameter says it
 * (0 = ○ confirms, 1 = ✕, as psputility_sysparam.h says); the XMB keeps it in the registry and the Impose service has the same pair of
 * values that Settings changes. */
int sys_confirm_is_circle(void)
{
    int v = 1;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &v) < 0)
        return 0;
    return v == 0;
}

/* One number in the system's registry, where the XMB keeps its own settings (the registry is the same place the system parameter above is read
 * from): `category`/`key` is set to `value`, opened for writing, written and flushed, the way the XMB and the homebrew tools that swap the buttons do
 * it. 1 when it was written, 0 when any step said no. */
static int reg_set_int(const char *category, const char *key, int value)
{
    struct RegParam reg;
    REGHANDLE h = 0, hd = 0;
    int ok = 0;
    memset(&reg, 0, sizeof(reg));
    reg.regtype = 1;
    reg.namelen = strlen(SYSTEM_REGISTRY);
    reg.unk2 = 1;
    reg.unk3 = 1;
    strcpy(reg.name, SYSTEM_REGISTRY);
    if (sceRegOpenRegistry(&reg, 2, &h) < 0)
        return 0;
    if (sceRegOpenCategory(h, category, 2, &hd) >= 0) {
        if (sceRegSetKeyValue(hd, key, &value, sizeof(value)) >= 0) {
            sceRegFlushCategory(hd);
            ok = 1;
        }
        sceRegCloseCategory(hd);
    }
    sceRegFlushRegistry(h);
    sceRegCloseRegistry(h);
    return ok;
}

/* Gives the confirm button to ○ (circle = 1) or ✕. It is saved the way the XMB saves it, in the registry (/CONFIG/SYSTEM/XMB, button_assign: 0 = ○,
 * 1 = ✕, the same numbers as the system parameter), so it stays when psp5 is closed and the XMB follows; the Impose service, which the programs that
 * are running ask, is told as well. The SDK does not say what values sceImposeSetLanguageMode wants ("parameter values not known"), so it is
 * calibrated first: the pair that sceImposeGetLanguageMode reports is compared with the system parameter, and when the button number is not the same
 * one (the other way round) it is turned round before it is written. What the system says afterwards is the answer, so a PSP that does not take the
 * change shows the old value. **Not tried on a PSP.** */
int sys_set_confirm_is_circle(int circle)
{
    int lang = 0, button = 0, param = 1;
    int have_impose = sceImposeGetLanguageMode(&lang, &button) >= 0;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &param) < 0)
        return sys_confirm_is_circle();
    int same = button == param; /* both numbers mean the same thing */
    int want = circle ? 0 : 1;  /* the system parameter's number for what is wanted */
    reg_set_int("/CONFIG/SYSTEM/XMB", "button_assign", want);
    if (have_impose)
        sceImposeSetLanguageMode(lang, same ? want : !want);
    int now = 1;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &now) >= 0 && now == want)
        return circle ? 1 : 0; /* the system says it: that is the answer */
    if (!have_impose)
        return sys_confirm_is_circle();
    int lang2 = 0, button2 = 0; /* ask the same service that was told: the system parameter may not follow until the next start */
    if (sceImposeGetLanguageMode(&lang2, &button2) < 0)
        return sys_confirm_is_circle();
    return (same ? button2 : !button2) == 0;
}

/* --- the PSP's own sound and power settings (what the XMB's Sound Settings and Power Save Settings change) ---
 *
 * The backlight timer has a call of its own that any program may use (sceImposeGetBacklightOffTime, in seconds). The sound settings are
 * parameters of the kernel's Impose service, which a program cannot call: ARK finds the function and kubridge calls it in kernel mode, the way
 * the volume bar (osd.c) reads the volume. The numbers of the functions are the old ones (sceImposeGetParam 0x531C9778, sceImposeSetParam
 * 0x810FB7FB, both in the SDK's stub library); ARK translates them for 6.60 / 6.61. The numbers of the parameters are in pspimpose_driver.h
 * (found in the 3.52 kernel: PSP_IMPOSE_MAIN_VOLUME 1, PSP_IMPOSE_BACKLIGHT_BRIGHTNESS 2, PSP_IMPOSE_EQUALIZER_MODE 4, PSP_IMPOSE_AVLS 0x10).
 * The values they take are written at each function (the volume is 0 to 30, as the volume bar reads it). A call that does not work answers
 * -1 and Settings leaves the row out. */
#define NID_IMPOSE_GET_PARAM     0x531C9778
#define NID_IMPOSE_GET_PARAM_660 0xDC3BECFF /* the same in 6.60 / 6.61, if ARK's translation is missing (as osd.c) */
#define NID_IMPOSE_SET_PARAM     0x810FB7FB
#define IMPOSE_MAIN_VOLUME       0x1
#define IMPOSE_BRIGHTNESS        0x2
#define IMPOSE_EQUALIZER_MODE    0x4
#define IMPOSE_AVLS              0x10

static void *s_impose_get, *s_impose_set;
static int s_impose_looked;

static void impose_find(void)
{
    if (s_impose_looked)
        return;
    s_impose_looked = 1;
    if (!s_has_ark)
        return;
    u32 g = sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_IMPOSE_GET_PARAM);
    if (!g)
        g = sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_IMPOSE_GET_PARAM_660);
    s_impose_get = (void *)g;
    s_impose_set = (void *)sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_IMPOSE_SET_PARAM);
}

#ifdef PSP5_TEST
/* The test build (PPSSPP has no Impose service, and no ARK): the settings it shows start like this and a change is kept, so that the pages of
 * Settings -> Display and Sound can be looked at and tried in the emulator. */
static int s_test_impose[0x20] = { [IMPOSE_MAIN_VOLUME] = 18, [IMPOSE_BRIGHTNESS] = 3 };
static int s_test_backlight = 120;
#endif

static int impose_get(int param)
{
#ifdef PSP5_TEST
    return s_test_impose[param & 0x1F];
#endif
    impose_find();
    if (!s_impose_get)
        return -1;
    KernelCallArg a = { 0 };
    a.arg1 = param;
    if (kuKernelCall(s_impose_get, &a) < 0)
        return -1;
    return (int)a.ret1;
}

static int impose_set(int param, int value)
{
#ifdef PSP5_TEST
    s_test_impose[param & 0x1F] = value;
    return 0;
#endif
    impose_find();
    if (!s_impose_set)
        return -1;
    KernelCallArg a = { 0 };
    a.arg1 = param;
    a.arg2 = value;
    if (kuKernelCall(s_impose_set, &a) < 0)
        return -1;
    return (int)a.ret1;
}

#ifdef PSP5_TEST
int sys_backlight_off(void) { return s_test_backlight; }

int sys_set_backlight_off(int seconds)
{
    s_test_backlight = seconds;
    return s_test_backlight;
}
#else
int sys_backlight_off(void) { return sceImposeGetBacklightOffTime(); }

int sys_set_backlight_off(int seconds)
{
    if (sceImposeGetBacklightOffTime() >= 0)
        sceImposeSetBacklightOffTime(seconds);
    return sceImposeGetBacklightOffTime(); /* what the PSP says now */
}
#endif

/* The volume (0 to 30, what the volume buttons and the volume bar have) and the brightness (the PSP's own levels: 0 to 3, as its brightness
 * button gives them; 4 blacks the screen out). The caller shows the level plus one, like the XMB. */
int sys_volume(void) { return impose_get(IMPOSE_MAIN_VOLUME); }

int sys_set_volume(int level)
{
    if (impose_get(IMPOSE_MAIN_VOLUME) >= 0)
        impose_set(IMPOSE_MAIN_VOLUME, level);
    return impose_get(IMPOSE_MAIN_VOLUME);
}

int sys_brightness(void) { return impose_get(IMPOSE_BRIGHTNESS); }

int sys_set_brightness(int level)
{
    if (impose_get(IMPOSE_BRIGHTNESS) >= 0)
        impose_set(IMPOSE_BRIGHTNESS, level);
    return impose_get(IMPOSE_BRIGHTNESS);
}

int sys_equalizer(void) { return impose_get(IMPOSE_EQUALIZER_MODE); }

int sys_set_equalizer(int mode)
{
    if (impose_get(IMPOSE_EQUALIZER_MODE) >= 0)
        impose_set(IMPOSE_EQUALIZER_MODE, mode);
    return impose_get(IMPOSE_EQUALIZER_MODE);
}

int sys_avls(void) { return impose_get(IMPOSE_AVLS); }

int sys_set_avls(int on)
{
    if (impose_get(IMPOSE_AVLS) >= 0)
        impose_set(IMPOSE_AVLS, on ? 1 : 0);
    return impose_get(IMPOSE_AVLS);
}

/* free and total space of a drive (ms0:, ef0:): the same call the file manager makes */
static int device_space(const char *dev, unsigned long long *free_b, unsigned long long *total_b)
{
    struct { u32 max_clusters, free_clusters, max_sectors, sector_size, sectors_per_cluster; } info;
    void *p = &info;
    if (sceIoDevctl(dev, 0x02425818, &p, sizeof(p), NULL, 0) < 0)
        return 0;
    unsigned long long cluster = (unsigned long long)info.sector_size * info.sectors_per_cluster;
    *free_b = (unsigned long long)info.free_clusters * cluster;
    *total_b = (unsigned long long)info.max_clusters * cluster;
    return 1;
}

/* scePowerGetBatteryTemp: the SDK says degrees Celsius, but a PSP gives the temperature in hundredths of a degree (36.87 °C comes as 3687 - psp5 showed
 * "3687 °C" first). A battery is never 100 °C, so a number from 100 up is hundredths and is rounded to whole degrees; below that it is taken as it
 * is (should a PSP give whole degrees); a negative number is the PSP not saying (-1). */
static int battery_celsius(int raw)
{
    if (raw < 0)
        return -1;
    return raw >= 100 ? raw / 100 + (raw % 100 >= 50) : raw;
}

void sys_about(SysAbout *a)
{
    memset(a, 0, sizeof(*a));
    a->percent = a->mv = a->celsius = a->mah_left = a->mah_full = -1;
    if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, a->nickname, sizeof(a->nickname)) < 0)
        a->nickname[0] = 0;
    a->nickname[sizeof(a->nickname) - 1] = 0;
    a->devkit = sceKernelDevkitVersion();
    a->has_mac = sceWlanGetEtherAddr(a->mac) >= 0;
    a->battery = scePowerIsBatteryExist() > 0;
    if (a->battery) {
        a->percent = scePowerGetBatteryLifePercent();
        a->charging = scePowerIsBatteryCharging() > 0;
        a->mv = scePowerGetBatteryVolt();
        a->celsius = battery_celsius(scePowerGetBatteryTemp());
        a->mah_left = scePowerGetBatteryRemainCapacity();
        a->mah_full = scePowerGetBatteryFullCapacity();
    }
    a->has_ms = device_space("ms0:", &a->ms_free, &a->ms_total);
    a->has_ef = s_model == 4 && device_space("ef0:", &a->ef_free, &a->ef_total); /* only a PSP Go has an internal storage */
}

int sys_has_ark(void) { return s_has_ark; }
int sys_ark_version(void) { return s_ark; }
int sys_model(void) { return s_model; }
int sys_ms_is_ef(void) { return s_ms_is_ef; }
int sys_is_psp1000(void) { return s_model == 0; }
const char *sys_ark_dir(void) { return s_ark_dir; }
int sys_is_launcher(void) { return s_is_launcher; }

static void settings_path(char *path, int size)
{
    snprintf(path, size, "%sSETTINGS.TXT", s_ark_dir);
}

/* ARK läser SETTINGS.TXT vid varje omstart och slår då på launcherläget igen, så
 * en utgång till XMB hamnar annars i psp5 direkt. Pausa läget i filen och lämna en
 * markering – nästa gång psp5 startas (från XMB) slås det på igen. */
static void pause_launcher(void)
{
    char path[200];
    settings_path(path, sizeof(path));
    if (!arkcfg_load(path) || !g_cfw.launcher)
        return;
    g_cfw.launcher = 0;
    if (!arkcfg_save(path))
        return;
    SceUID fd = sceIoOpen(paused_mark(), PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0)
        sceIoClose(fd);
}

void sys_resume_launcher(void)
{
    if (!s_has_ark || !exists(paused_mark()))
        return;
    char path[200];
    settings_path(path, sizeof(path));
    if (arkcfg_load(path)) {
        g_cfw.launcher = 1;
        if (!arkcfg_save(path))
            return; /* försök igen nästa start */
    }
    sceIoRemove(paused_mark());
}

void sys_exit_xmb(void)
{
    if (!s_has_ark) {
        sceKernelExitGame();
        return;
    }
    pause_launcher();
    if (s_ark >= 5) {
        /* stäng också av läget i minnet, så att inget i den här sessionen startar launchern.
         * (ARK-4 har en annan SEConfig-layout och läser ändå om SETTINGS.TXT vid omstarten.) */
        SEConfigARK se;
        memset(&se, 0, sizeof(se));
        if (sctrlSEGetConfigEx((SEConfig *)&se, sizeof(se)) >= 0 && se.magic == ARK_CONFIG_MAGIC) {
            se.launcher_mode = 0;
            sctrlSESetConfigEx((SEConfig *)&se, sizeof(se));
        }
#ifndef PSP5_ARK4
        if (IS_ARK_CONFIG(&s_conf)) { /* skriv aldrig tillbaka en konfiguration som inte gick att läsa */
            s_conf.launcher[0] = 0;
            sctrlArkSetConfig(&s_conf);
        }
#endif
    }
    sctrlKernelExitVSH(NULL);
}

void sys_restart(void) { scePowerRequestColdReset(0); }
void sys_suspend(void) { scePowerRequestSuspend(); }
void sys_shutdown(void) { scePowerRequestStandby(); }

static int exists(const char *path)
{
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

void sys_recovery(void)
{
    if (!s_has_ark)
        return;
    char path[200];
    /* som arkMenu: RECOVERY.PBP om den finns, annars den klassiska PRX-menyn */
    snprintf(path, sizeof(path), "%sRECOVERY.PBP", s_ark_dir);
    if (exists(path)) {
        struct SceKernelLoadExecVSHParam param;
        memset(&param, 0, sizeof(param));
        param.args = strlen(path) + 1;
        param.argp = path;
        param.key = "game";
        sctrlKernelLoadExecVSHWithApitype(0x141, path, &param);
        return;
    }
    snprintf(path, sizeof(path), "%sRECOVERY.PRX", s_ark_dir);
    if (!exists(path))
        snprintf(path, sizeof(path), "%s", RECOVERY_PRX_FLASH);
    SceUID mod = kuKernelLoadModule(path, 0, NULL);
    if (mod >= 0 && sceKernelStartModule(mod, strlen(path) + 1, path, NULL, NULL) >= 0) {
        for (;;)
            sceKernelDelayThread(1000000); /* återställningsmenyn tar över */
    }
}

int sys_usb_start(void)
{
    static const char *mods[] = {
        "flash0:/kd/semawm.prx",     "flash0:/kd/usbstor.prx",      "flash0:/kd/usbstormgr.prx",
        "flash0:/kd/usbstorms.prx",  "flash0:/kd/usbstoreflash.prx", "flash0:/kd/usbstorboot.prx",
    };
    static int loaded;
    if (s_usb_on || !s_has_ark)
        return s_usb_on;
    if (!loaded) {
        for (unsigned i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
            SceUID mid = kuKernelLoadModule(mods[i], 0, NULL);
            if (mid >= 0)
                sceKernelStartModule(mid, 0, NULL, NULL, NULL);
        }
        loaded = 1;
    }
    sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
    sceUsbStart(USBSTOR_EF_DRIVERNAME, 0, 0);
    /* As arkMenu does: the result is ignored. Giving up on an error here closed the USB mode at once on a PSP Go (ARK-4). */
    sceUsbActivate(0x1c8);
    s_usb_on = 1;
    return s_usb_on;
}

void sys_usb_stop(void)
{
    if (!s_usb_on)
        return;
    sceUsbDeactivate(0x1c8);
    sceUsbStop(USBSTOR_EF_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    s_usb_on = 0;
}

int sys_usb_state(void)
{
    if (!s_usb_on)
        return 0;
    return (sceUsbGetState() & PSP_USB_CONNECTION_ESTABLISHED) ? 2 : 1;
}
