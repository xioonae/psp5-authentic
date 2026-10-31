/* psp5 – made by Mille Weber */
/* The PSP's own screen for choosing a Wi-Fi connection (see wifi.h): the network dialog of the utility library, started and drawn the way
 * the on-screen keyboard of the file manager is (files.c): the program draws a frame, and between its drawing and the swap of the buffers the
 * dialog is stepped (gfx_end_with). The dialog needs the network libraries started first, and they are stopped again at the end.
 * **Not run on a PSP.** What the SDK offers is "connect to an access point" (choose one of the saved connections and connect), "show the
 * status" and ad hoc; it has no way to ask for adding, editing or removing a connection. */
#include <pspkernel.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <psputility_netconf.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <string.h>
#include "wifi.h"
#include "netlink.h"
#include "gfx.h"
#include "theme.h"

#define MODULE_ALREADY_LOADED 0x80111102 /* the answer of a load that was done before (music.c has the same) */

static int s_done;

static void step(void)
{
    switch (sceUtilityNetconfGetStatus()) {
    case PSP_UTILITY_DIALOG_VISIBLE: sceUtilityNetconfUpdate(1); break;
    case PSP_UTILITY_DIALOG_QUIT:    sceUtilityNetconfShutdownStart(); break;
    case PSP_UTILITY_DIALOG_NONE:    s_done = 1; break;
    }
}

/* the background while the dialog is up: the theme's scene, as in the other full-screen modes */
static void draw_frame(float t)
{
    gfx_begin(RGBA(20, 20, 24, 255));
    theme_backdrop(theme_cur(), t, 1);
}

/* The dialog itself (the network libraries are running): 1 = a connection was chosen and made, 0 = the player closed it without one, -1 = it would not start. */
static int run_dialog(volatile int *running)
{
    int result = -1;
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhoc;
    memset(&data, 0, sizeof(data));
    memset(&adhoc, 0, sizeof(adhoc));
    data.base.size = sizeof(data);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &data.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &data.base.buttonSwap); /* the dialog is the system's: it follows the system's setting */
    data.base.graphicsThread = 17;
    data.base.accessThread = 19;
    data.base.fontThread = 18;
    data.base.soundThread = 16;
    data.action = PSP_NETCONF_ACTION_CONNECTAP;
    data.adhocparam = &adhoc;
    data.hotspot = 1; /* all kinds of saved connections can be chosen */
    data.wifisp = 1;
    if (sceUtilityNetconfInitStart(&data) >= 0) {
        float t = 0;
        s_done = 0;
        while (!s_done && *running) {
            draw_frame(t);
            gfx_end_with(step);
            t += 1 / 60.0f;
        }
        for (int i = 0; i < 120 && !s_done; i++) { /* closed from outside (HOME): let the dialog end properly */
            if (sceUtilityNetconfGetStatus() == PSP_UTILITY_DIALOG_VISIBLE)
                sceUtilityNetconfShutdownStart();
            draw_frame(t);
            gfx_end_with(step);
        }
        result = data.base.result == 0 ? 1 : 0;
    }
    return result;
}

int wifi_dialog(volatile int *running)
{
    int common = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    int inet = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    int net = -1, inet_init = -1, apctl = -1, result = -1;
    if ((common >= 0 || common == (int)MODULE_ALREADY_LOADED) && (inet >= 0 || inet == (int)MODULE_ALREADY_LOADED) &&
        (net = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000)) >= 0 && (inet_init = sceNetInetInit()) >= 0 && (apctl = sceNetApctlInit(0x1600, 42)) >= 0)
        result = run_dialog(running);
    if (apctl >= 0)
        sceNetApctlTerm();
    if (inet_init >= 0)
        sceNetInetTerm();
    if (net >= 0)
        sceNetTerm();
    if (inet >= 0)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    if (common >= 0)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    return result;
}

int wifi_connect(volatile int *running)
{
    int result = -1;
    if (netlink_start()) {
        result = run_dialog(running);
        if (result == 1) {
            netlink_attach();
            return 1;
        }
    }
    netlink_stop();
    return result;
}
