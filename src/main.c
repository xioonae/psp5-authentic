/* psp5 – made by Mille Weber */
/* psp5 – egen launcher för PSP med ARK-5. Kan köras som vanlig app
 * (ms0:/PSP/GAME/PSP5) eller som ARK:s launcher (ARK_01234/VBOOT.PBP). */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psputility.h>
#include <psppower.h>
#include <psprtc.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "gfx.h"
#include "scan.h"
#include "art.h"
#include "loader.h"
#include "ui.h"
#include "launch.h"
#include "settings.h"
#include "system.h"
#include "testrun.h"
#include "sound.h"
#include "lang.h"
#include "config.h"
#include "controls.h"
#include "boot.h"
#include "music.h"
#include "umd.h"
#include "theme.h"
#include "osd.h"
#include "files.h"
#include "wifi.h"
#include "info.h"
#include "gameops.h"
#include "saves.h"
#include "home.h"
#include "playtime.h"
#include "lastplayed.h"
#include "power.h"
#include "helper.h"

PSP_MODULE_INFO("psp5", PSP_MODULE_USER, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
/* Lämna 7 MB fritt: ARK:s RECOVERY.PRX (4 MB egen heap), MP3-avkodarens moduler och
 * trådarnas stackar. Resten (~17 MB på PSP-1000) räcker gott – psp5 använder under 10 MB. */
PSP_HEAP_SIZE_KB(-7168);

/* psp5:s egna filer (omslag, cache) – på lagringen psp5 startades från (ms0: eller PSP Go:s ef0:) */
#define DATA_DIR PSP5_DATA_DIR
/* Finns medan ett spel körs: när spelet avslutas startar systemet om psp5
 * (som ARK:s launcher), och då ska startskärmen inte visas igen. */
#define INGAME_MARK s_ingame_mark
/* Startskydd: filen finns medan psp5 startar och tas bort när menyn har gått en stund.
 * Blir den kvar har psp5 hängt sig eller kraschat under starten. Då blir nästa start
 * felsäker, och som ARK:s launcher går psp5 efter två sådana starter i rad direkt
 * till XMB – så att PSP:n aldrig fastnar i en start som inte fungerar. */
#define START_MARK s_start_mark
#define START_OK_AFTER 5.0f /* sekunder i menyn innan starten räknas som klar */

static volatile int s_running = 1;
static int s_safe;     /* felsäker start: ingen startskärm, musik eller UMD, cachen tömd */
static int s_start_ok; /* starten är klar och startskyddet borttaget */
static volatile int s_sleeping; /* PSP:n går in i viloläge – skärmen hålls svart */
static volatile int s_resumed;  /* PSP:n har vaknat klart – visa startskärmen */
static char s_self[256];
static char s_list_cache[256];
static char s_ingame_mark[32], s_start_mark[32], s_cache_dir[32]; /* i DATA_DIR, sätts i main */
static u64 s_scan_start;

static int exit_callback(int arg1, int arg2, void *common)
{
    s_running = 0;
    return 0;
}

static int power_callback(int unknown, int flags, void *common)
{
    if (flags & PSP_POWER_CB_RESUME_COMPLETE)
        s_resumed = 1;
    else if (flags & PSP_POWER_CB_SUSPENDING)
        s_sleeping = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cb = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cb);
    int pcb = sceKernelCreateCallback("Power Callback", power_callback, NULL);
    scePowerRegisterCallback(-1, pcb);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int th = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (th >= 0)
        sceKernelStartThread(th, 0, NULL);
}

static void scan_progress(int done, const char *name)
{
    ui_draw_scanning(done, name, (sceKernelGetSystemTimeWide() - s_scan_start) / 1000000.0f);
}

static int has_non_ascii(const EntryList *list)
{
    for (int i = 0; i < list->count; i++)
        for (const char *p = list->items[i].title; *p; p++)
            if ((u8)*p >= 0x80)
                return 1;
    return 0;
}

/* The marker of a game that is running: it is there when psp5 is started again after a game, and then psp5 skips its start-up screen. */
static int take_ingame_mark(void)
{
    SceIoStat st;
    if (sceIoGetstat(INGAME_MARK, &st) < 0)
        return 0;
    sceIoRemove(INGAME_MARK);
    return 1;
}

static void set_ingame_mark(void)
{
    if (!sys_is_launcher())
        return; /* som vanlig app går systemet till XMB efter spelet */
    SceUID fd = sceIoOpen(INGAME_MARK, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0)
        sceIoClose(fd);
}

static void start_write(int count)
{
    char c = '0' + (count > 9 ? 9 : count);
    SceUID fd = sceIoOpen(START_MARK, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &c, 1);
        sceIoClose(fd);
    }
}

/* Hur många starter i rad som inte blev klara. Räknar samtidigt upp för den här. */
static int start_begin(void)
{
    int failed = 0;
    SceUID fd = sceIoOpen(START_MARK, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        char c = 0;
        failed = 1; /* filen finns: minst en start blev inte klar */
        if (sceIoRead(fd, &c, 1) == 1 && c >= '1' && c <= '9')
            failed = c - '0';
        sceIoClose(fd);
    }
    start_write(failed + 1);
    return failed;
}

/* psp5 har startat klart: ta bort startskyddet och slå på launcherläget igen om det var pausat. */
static void start_confirmed(void)
{
    if (s_start_ok)
        return;
    s_start_ok = 1;
    sceIoRemove(START_MARK);
    sys_resume_launcher();
}

/* Bygger upp allt: grafik, typsnitt, spellista, laddartråd. */
static void app_start(EntryList *list, Ui *ui, int boot)
{
    gfx_init();
    font_init(lang_needs_cjk());
    sound_init();
    if (!s_safe)
        umd_start(); /* skivan hinner snurra upp under startskärmen */
    if (boot && g_boot_anim && !s_safe)
        boot_run(&s_running); /* startskärmen, innan spelen läses in */
    s_scan_start = sceKernelGetSystemTimeWide();
    scan_all(list, s_self, s_list_cache, scan_progress);
    Entry disc;
    int has_disc;
    if (umd_poll(&disc, &has_disc) && has_disc)
        scan_insert_front(list, &disc); /* UMD-skivan först */
    if (has_non_ascii(list))
        font_ensure_cjk(); /* t.ex. japanska speltitlar */
    loader_start(list);
    if (!s_safe)
        music_init(); /* läser in ms0:/MUSIC i bakgrunden */
    controls_psp_assignment(sys_confirm_is_circle()); /* what the PSP's own setting says; Settings > Controls may have other buttons do the jobs */
    ui_init(ui, list, launch_supported());
}

/* Viloläge med sidoknappen: när PSP:n somnar ritas skärmen svart, så att det är
 * svart som syns när den vaknar (inte menyn). Svart tills PSP:n har vaknat klart,
 * sedan startskärmen. Musiken pausas under tiden. */
#define WAKE_TIMEOUT 4.0f /* sekunder vaken utan "vaknat klart" – visa startskärmen ändå */

static void sleep_and_wake(Ui *ui)
{
    int music = music_state() == MUSIC_PLAYING;
    if (music)
        music_toggle_pause();
    sound_prepare_boot(); /* startljudet frigjordes efter förra startskärmen */

    /* Svart i båda bildbuffertarna, sedan vilar psp5 utan att rita: grafikkretsen ska
     * inte arbeta medan PSP:n somnar och vaknar. Har vi inte sett PSP:n somna (t.ex.
     * viloläge från inställningarna) blir det direkt startskärm. */
    if (s_sleeping) {
        for (int i = 0; i < 2; i++) {
            gfx_begin(RGBA(0, 0, 0, 255));
            gfx_end();
        }
    }
    u64 last = sceKernelGetSystemTimeWide();
    float awake = 0; /* systemtiden står still medan PSP:n sover */
    while (s_sleeping && s_running && !s_resumed && awake < WAKE_TIMEOUT) {
        sceKernelDelayThread(20000);
        u64 now = sceKernelGetSystemTimeWide();
        float dt = (now - last) / 1000000.0f;
        last = now;
        awake += dt < 0.1f ? dt : 0.1f;
        testrun_buttons(0); /* testbygget: skriptet går vidare */
        if (testrun_take_resume())
            s_resumed = 1;
    }
    s_sleeping = s_resumed = 0;
    theme_backdrop_forget(); /* the video memory may not have been kept through the sleep */

    boot_run(&s_running);
    if (music)
        music_toggle_pause();
    ui_replay_intro(ui);
}

/* Frigör allt i omvänd ordning – inför spelstart, USB-läge eller avslut. */
static void app_stop(EntryList *list, Ui *ui)
{
    umd_stop();
    loader_stop(); /* före music_term: laddaren läser musiklistan (omslagen) */
    music_term();
    sound_term();
    ui_free(ui);
    font_term();
    gfx_term();
    scan_free(list);
}

/* UMD-skivan har satts i eller tagits ut: lägg in den först i listan eller ta bort den. */
static void umd_refresh(EntryList *list, Ui *ui)
{
    Entry disc;
    int has;
    if (!umd_poll(&disc, &has))
        return;
    int had = list->count > 0 && list->items[0].type == ENTRY_UMD;
    if (!had && !has)
        return;
    loader_stop(); /* laddarens platser följer listans index */
    if (had)
        scan_remove_front(list);
    if (has && !scan_insert_front(list, &disc))
        has = 0;
    loader_start(list);
    ui_relist(ui, had, has);
    if (has && has_non_ascii(list))
        font_ensure_cjk();
}

/* Ett spel har raderats (helt eller bara delvis): ta bort det ur listan, om huvudfilen är borta, och starta
 * om laddaren (den stoppades innan raderingen). `index` är spelets plats i listan. */
static void delete_finished(EntryList *list, Ui *ui, int index, int result)
{
    Entry gone = list->items[index];
    int removed = result == DEL_OK || !gameops_still_exists(&gone);
    if (removed) {
        ui_forget_entry(ui, &list->items[index]);
        art_forget(&gone);
        scan_remove_at(list, index);
    }
    loader_start(list);
    if (removed)
        ui_relist_removed(ui, index);
    else
        ui_relist(ui, 0, 0);
    if (result == DEL_OK) {
        ui_toast(ui, L(S_DELETED));
        sound_play(SND_SELECT);
    } else {
        ui_toast(ui, L(result == DEL_CANCELLED ? S_F_CANCELLED : S_F_FAILED));
        sound_play(SND_ERROR);
    }
}

/* The pad as the screens see it: controls_map() turns the buttons that are pressed into the names the screens use (confirm, back, options, the
 * power menu ...). `*old` and `s_raw` are the buttons as they are on the pad; the new presses are found there and mapped after, so a button that is
 * held while Settings gives the buttons other jobs does not count as a new press. */
static u32 s_raw;

static void read_pad(UiInput *in, u32 *old)
{
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    pad.Buttons = testrun_buttons(pad.Buttons); /* bara i testbyggen */
    in->pressed = controls_map(pad.Buttons & ~*old);
    in->held = controls_map(pad.Buttons);
    s_raw = pad.Buttons;
    in->lx = pad.Lx;
    in->ly = pad.Ly;
    *old = pad.Buttons;
}

/* The power popup of the SELECT button on the screens that are not Games, Library and Music (those have it in ui.c): SELECT opens it, unless `can_open` is 0 (the
 * screen has a popup of its own or a job running); while it is open the screen gets no buttons (`in` is cleared, also for the press that closes it). Returns what the player
 * chose (SA_XMB, SA_SUSPEND, SA_SHUTDOWN, SA_RESTART, SA_RECOVERY), else SA_NONE. Call it every frame, before the screen's own update. */
static int power_step(UiInput *in, float dt, int can_open)
{
    int act = power_frame(in, dt, can_open);
    if (act != SA_NONE)
        sound_wait(400); /* let the sound be heard before anything closes, as on the main screen */
    return act;
}

/* USB-läge: inget annat rör minneskortet medan datorn är ansluten. Returns what the player chose in the power popup there (SA_*), else 0: USB is stopped first, and the caller does it. */
static int usb_mode(void)
{
    gfx_init();
    font_init(lang_needs_cjk());
    if (!sys_usb_start()) {
        gfx_term();
        font_term();
        return SA_NONE;
    }
    power_reset();
    int pending = SA_NONE;
    u32 old = ~0u; /* ignore the button that started the mode, whichever it is (the confirm button) */
    float t = 0;
    while (s_running) {
        UiInput in;
        read_pad(&in, &old);
        pending = power_step(&in, 1 / 60.0f, 1);
        if (pending != SA_NONE)
            break;
        if (in.pressed & PSP_CTRL_CIRCLE) /* back */
            break;
        t += 1 / 60.0f;
        osd_update(1 / 60.0f);

        int state = sys_usb_state();
        gfx_begin(RGBA(20, 20, 24, 255));
        theme_backdrop(theme_cur(), t, 1); /* som inställningarna */
        text_draw(28, 34, 0.95f, 0, RGBA(238, 238, 242, 255), ALIGN_LEFT, L(S_USB_MODE));
        float pulse = 0.6f + 0.4f * sinf(t * 3);
        gfx_circle(SCREEN_W / 2, 120, 26, RGBA(255, 255, 255, state == 2 ? 40 : (int)(30 * pulse)));
        gfx_circle(SCREEN_W / 2, 120, 9, state == 2 ? RGBA(120, 220, 150, 255) : RGBA(236, 236, 240, (int)(255 * pulse)));
        text_draw(SCREEN_W / 2, 176, 0.75f, 0, RGBA(238, 238, 242, 255), ALIGN_CENTER,
                  L(state == 2 ? S_USB_CONNECTED : S_USB_WAITING));
        text_draw(SCREEN_W / 2, 196, 0.56f, 0, RGBA(160, 162, 172, 255), ALIGN_CENTER,
                  L(S_USB_EJECT));
        { /* knapptips som i filhanteraren: en mörk kapsel nere till höger */
            int btn = controls_glyph(PSP_CTRL_CIRCLE);
            float bw = gfx_button_width(btn), w = bw + 5 + ceilf(text_width(0.56f, 0, L(S_USB_EXIT))), x = SCREEN_W - 22 - w, y = 252; /* (whole pixels) */
            gfx_round_rect(x - 9, y - 10, w + 18, 20, 10, RGBA(0, 0, 0, 110));
            gfx_button(x + bw / 2, y, 6.5f, btn, RGBA(236, 236, 240, 255), RGBA(20, 20, 26, 255));
            text_draw(x + bw + 5, y + 4.5f, 0.56f, 0, RGBA(238, 238, 242, 255), ALIGN_LEFT, L(S_USB_EXIT));
        }
        power_draw();
        osd_draw();
        gfx_end();
    }
    sys_usb_stop();
    s_sleeping = s_resumed = 0; /* viloläge under USB-läget: ingen startskärm efteråt */
    font_term();
    gfx_term();
    return pending;
}

/* The PSP's own screen for choosing a Wi-Fi connection (wifi.c). Everything else is closed, as in the other modes. s_wifi_failed: the network could not be started. */
static int s_wifi_failed;

static void wifi_mode(void)
{
    gfx_init();
    s_wifi_failed = wifi_dialog(&s_running) < 0;
    s_sleeping = s_resumed = 0;
    gfx_term();
}

/* Filhanteraren: allt annat är stängt, så inget annat läser eller skriver filer under tiden. Returns what the player chose in the power popup there that leaves
 * psp5 (SA_SHUTDOWN, SA_RESTART, SA_RECOVERY), else 0: the File Manager is closed first, and the caller does it (Sleep is done on the spot, as with the PSP's own switch). */
static int files_mode(void)
{
    gfx_init();
    font_init(lang_needs_cjk());
    sound_init();
    files_open();
    power_reset();
    int pending = SA_NONE;
    u32 old = ~0u; /* ignore the button that opened it, whichever it is (the confirm button) */
    u64 last = sceKernelGetSystemTimeWide();
    while (s_running) {
        UiInput in;
        read_pad(&in, &old);
        u64 now = sceKernelGetSystemTimeWide();
        float dt = (now - last) / 1000000.0f;
        last = now;
        if (dt > 0.05f)
            dt = 0.05f;
        osd_update(dt);
        int pwr = power_step(&in, dt, !files_modal());
        if (pwr == SA_SUSPEND) {
            sys_suspend();
            old = s_raw;
        } else if (pwr != SA_NONE) {
            pending = pwr;
            break;
        }
        if (files_update(&in, dt))
            break;
        files_draw();
    }
    files_close();
    sound_term();
    s_sleeping = s_resumed = 0;
    font_term();
    gfx_term();
    return pending;
}

static void delete_dir_files(const char *dir)
{
    SceUID d = sceIoDopen(dir);
    if (d < 0)
        return;
    SceIoDirent ent;
    char path[256];
    memset(&ent, 0, sizeof(ent));
    while (sceIoDread(d, &ent) > 0) {
        if (!FIO_S_ISDIR(ent.d_stat.st_mode)) {
            snprintf(path, sizeof(path), "%s/%s", dir, ent.d_name);
            sceIoRemove(path);
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
}

/* An action that leaves psp5 (everything of psp5 is stopped already): the system takes over. */
static void leave_for(int action)
{
    switch (action) {
    case SA_XMB:      sceIoRemove(START_MARK); sys_exit_xmb(); break; /* (the start guard goes, the launcher mode stays paused: sys_exit_xmb) */
    case SA_RESTART:  sys_restart(); break;
    case SA_SHUTDOWN: sys_shutdown(); break;
    case SA_RECOVERY: sys_recovery(); break;
    }
    for (;;)
        sceKernelDelayThread(100000); /* systemet tar över */
}

/* The File Manager or the USB screen is over and the player chose something in the power popup there (SA_NONE = nothing): Sleep comes back, the others leave psp5. */
static void after_visit(int action)
{
    if (action == SA_SUSPEND)
        sys_suspend();
    else if (action != SA_NONE)
        leave_for(action);
}

/* Utför en åtgärd från inställningarna. Returnerar 1 om appen ska byggas upp igen. */
static int run_action(int action, EntryList *list, Ui *ui)
{
    /* psp5 fungerar uppenbarligen. Till XMB tas bara startskyddet bort: launcherläget
     * ska vara fortsatt pausat där. */
    if (action == SA_XMB)
        sceIoRemove(START_MARK);
    else
        start_confirmed();
    switch (action) {
    case SA_SUSPEND:
        sys_suspend();
        return 0;
    case SA_USB:
        app_stop(list, ui);
        after_visit(usb_mode());
        return 1; /* filer kan ha ändrats – skanna om */
    case SA_FILES:
        app_stop(list, ui);
        after_visit(files_mode());
        return 1;
    case SA_WIFI:
        app_stop(list, ui);
        wifi_mode();
        return 1;
    case SA_CLEAR_CACHE:
        app_stop(list, ui);
        delete_dir_files(s_cache_dir);
        return 1;
    }
    /* resten lämnar psp5 – frigör allt först */
    app_stop(list, ui);
    leave_for(action);
    return 0;
}

int main(int argc, char *argv[])
{
    setup_callbacks();
    snprintf(s_self, sizeof(s_self), "%s", argc > 0 && argv[0] ? argv[0] : "ms0:/PSP/GAME/PSP5/EBOOT.PBP");
    sys_set_root(s_self);
    snprintf(s_ingame_mark, sizeof(s_ingame_mark), "%s/ingame", DATA_DIR);
    snprintf(s_start_mark, sizeof(s_start_mark), "%s/starting", DATA_DIR);
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s/cache", DATA_DIR);
    snprintf(s_list_cache, sizeof(s_list_cache), "%s/cache/list.bin", DATA_DIR);
    sceIoMkdir(DATA_DIR, 0777);
    int failed = start_begin(); /* först av allt – före allt som kan hänga sig */
    art_init(DATA_DIR);
    config_load(DATA_DIR); /* ljud och språk */
    sys_init(s_self);
    if (g_backlight_off)
        sys_set_backlight_off(g_backlight_off); /* the time chosen in Settings: the PSP forgets it at a restart */
    osd_init(); /* volymindikatorn behöver ARK */
    testrun_init();

    if (failed >= 2 && sys_is_launcher() && sys_has_ark()) {
        /* Två starter i rad har inte blivit klara: lämna PSP:n till XMB. Nästa start
         * som launcher blir felsäker (inte XMB direkt igen). */
        start_write(1);
        sys_exit_xmb();
        sceKernelExitGame();
        return 0;
    }
    s_safe = failed > 0;
    if (s_safe) {
        delete_dir_files(s_cache_dir); /* en trasig cachefil ska inte kunna stoppa starten */
        sound_skip_boot();
    }

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    EntryList list = { 0 };
    Ui ui;
    helper_setup(); /* the plugin that counts the time played: written / replaced, and put in PLUGINS.TXT the first time */
    playtime_set_self(s_self);
    lastplayed_load(DATA_DIR); /* when each game was played last (the plugin's files add the games that were started from the XMB) */
    playtime_load(DATA_DIR); /* adds the time that the plugin has kept since psp5 was last open */
    app_start(&list, &ui, !take_ingame_mark()); /* ingen startskärm efter ett spel */
    if (s_safe)
        ui_toast(&ui, L(S_SAFE_START));
#ifdef PSP5_TEST
    sound_dump("ms0:/PSP5/shots/sounds"); /* testbygge: ljuden som WAV */
#endif

    s_sleeping = s_resumed = 0; /* viloläge under startskärmen räknas inte */
    int in_settings = 0;
    int in_info = 0;
    int in_saves = 0;
    int deleting = -1; /* spelets plats i listan medan det raderas, annars -1 */
    u32 old = 0;
    float up = 0; /* sekunder i menyn – för startskyddet */
    u64 last = sceKernelGetSystemTimeWide();
    while (s_running) {
        if (testrun_take_sleep())
            s_sleeping = 1;
        if (testrun_take_resume())
            s_resumed = 1;
        if (s_sleeping || s_resumed) {
            if (g_boot_anim && ui.launch < 0 && !s_safe) {
                sleep_and_wake(&ui);
                old = ~0u; /* knappen som hoppade över startskärmen räknas inte */
                last = sceKernelGetSystemTimeWide();
            } else {
                s_sleeping = s_resumed = 0;
            }
        }
        UiInput in;
        read_pad(&in, &old);

        u64 now = sceKernelGetSystemTimeWide();
        float dt = (now - last) / 1000000.0f;
        last = now;
        if (dt > 0.05f)
            dt = 0.05f;
        osd_update(dt);
        if (!s_start_ok) {
            up += dt;
            if (up >= START_OK_AFTER && !music_scanning())
                start_confirmed(); /* menyn och musiklistan fungerar */
        }

        if (in_settings) {
            int pwr = power_step(&in, dt, !settings_modal()); /* SELECT: the power popup, as on the main screen */
            int action = settings_update(&in, dt);
            if (pwr != SA_NONE)
                action = pwr;
            if (action == SA_CLOSE) {
                in_settings = 0;
                if (settings_commit())
                    ui_toast(&ui, L(S_SAVED));
            } else if (action != SA_NONE) {
                settings_commit();
                if (run_action(action, &list, &ui)) {
                    app_start(&list, &ui, 0);
                    if (settings_is_visit(action)) { /* the PSP's own screen, the File Manager, USB Mode: back in Settings, on the same row (not the main screen) */
                        if (action == SA_WIFI && s_wifi_failed)
                            settings_notice(L(S_WIFI_FAILED));
                    } else {
                        in_settings = 0;
                    }
                }
                old = s_raw;
                continue;
            }
            /* bilder och bakgrund fortsätter laddas i bakgrunden */
            UiInput none = UI_NO_INPUT;
            ui_update(&ui, &none, dt);
            if (in_settings)
                settings_draw();
            else
                ui_draw(&ui);
            continue;
        }

        /* informationsskärmen och en pågående radering: spellistan får inte ändras under tiden (ingen
         * UMD-uppdatering), bilder och bakgrund fortsätter laddas */
        if (in_info) {
            UiInput none = UI_NO_INPUT;
            int pwr = power_step(&in, dt, 1);
            int close = info_update(&in, dt);
            ui_update(&ui, &none, dt);
            if (pwr != SA_NONE) { /* chosen in the power popup (SELECT): Sleep comes back to this screen, the others leave psp5 */
                if (pwr != SA_SUSPEND)
                    info_close();
                run_action(pwr, &list, &ui);
                old = s_raw;
                continue;
            }
            if (close) {
                info_close();
                in_info = 0;
                ui_draw(&ui);
            } else {
                info_draw();
            }
            continue;
        }
        if (in_saves) { /* sparad data: samma sak, listan står still medan skärmen är öppen */
            UiInput none = UI_NO_INPUT;
            int pwr = power_step(&in, dt, !saves_modal());
            int close = saves_update(&in, dt);
            ui_update(&ui, &none, dt);
            if (pwr != SA_NONE) { /* chosen in the power popup (SELECT): Sleep comes back to this screen, the others leave psp5 */
                if (pwr != SA_SUSPEND)
                    saves_close();
                run_action(pwr, &list, &ui);
                old = s_raw;
                continue;
            }
            if (close) {
                saves_close();
                in_saves = 0;
                ui_draw(&ui);
            } else {
                saves_draw();
            }
            continue;
        }
        if (deleting >= 0) {
            UiInput none = UI_NO_INPUT;
            int r = gameops_delete_update(in.pressed);
            ui_update(&ui, &none, dt);
            ui_draw(&ui);
            if (r != DEL_RUNNING) {
                gameops_delete_end();
                delete_finished(&list, &ui, deleting, r);
                deleting = -1;
            }
            continue;
        }

        if (ui.launch < 0)
            umd_refresh(&list, &ui);
        int action = ui_update(&ui, &in, dt);
        if (action == UI_SETTINGS) {
            /* The menu works: switch on a paused launcher mode before Settings reads it, or "Boot psp5" shows as off until the
             * first game is started. */
            start_confirmed();
            power_reset();
            settings_open();
            /* The sound comes after the reading (the font from flash0, ARK's files): heavy reading while it played made a
             * scratching noise for about 0.1 s. */
            sound_play(SND_SELECT);
            in_settings = 1;
            continue;
        }
        if (action == UI_XMB || action == UI_SLEEP || action == UI_SHUTDOWN || action == UI_RESTART || action == UI_RECOVERY) {
            /* the power popup of the SELECT button, the same choices as on the other screens (only sleep comes back) */
            run_action(action == UI_XMB ? SA_XMB : action == UI_SLEEP ? SA_SUSPEND : action == UI_SHUTDOWN ? SA_SHUTDOWN : action == UI_RESTART ? SA_RESTART : SA_RECOVERY,
                       &list, &ui);
            old = s_raw;
            continue;
        }
        if (action == UI_INFO && ui_selected(&ui) >= 0) {
            power_reset();
            info_open(&list.items[ui_selected(&ui)], ui_selected(&ui));
            in_info = 1;
            continue;
        }
        if (action == UI_SAVES) {
            power_reset();
            saves_open(&list);
            in_saves = 1;
            continue;
        }
        if (action == UI_DELETE && ui_selected(&ui) >= 0) {
            int index = ui_selected(&ui);
            loader_stop(); /* inget annat får läsa filen som raderas */
            if (gameops_delete_start(&list.items[index])) {
                deleting = index;
            } else {
                loader_start(&list);
                ui_relist(&ui, 0, 0);
                ui_toast(&ui, L(S_F_FAILED));
                sound_play(SND_ERROR);
            }
            continue;
        }
        if (action == UI_LAUNCH && ui_selected(&ui) >= 0) {
            Entry chosen = list.items[ui_selected(&ui)];
            home_note_launch(&chosen); /* the card becomes the newest one on Home */
            lastplayed_note_launch(&chosen); /* ... and the one that was played last (Library, Recently played) */
            sound_wait(700);      /* låt startljudet klinga ut */
            app_stop(&list, &ui); /* frigör allt minne innan spelet laddas */
            start_confirmed();    /* ett spel som hänger sig räknas inte mot psp5 */
            set_ingame_mark();
            launch_entry(&chosen);
            /* hit kommer vi bara om starten misslyckades */
            sceIoRemove(INGAME_MARK);
            app_start(&list, &ui, 0);
            ui_toast(&ui, L(S_LAUNCH_FAILED));
            continue;
        }
        ui_draw(&ui);
    }

    /* HOME → Avsluta: till XMB (även när psp5 är ARK:s launcher) */
    if (in_saves)
        saves_close(); /* avbryter en pågående radering */
    app_stop(&list, &ui);
    sceIoRemove(START_MARK);
    sys_exit_xmb();
    sceKernelExitGame();
    return 0;
}
