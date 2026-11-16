/* psp5-authentic – the connection to a Wi-Fi network (see netlink.h): the network libraries are started the way a game does it; the PSP's own window (wifi.c) joins the
 * access point. */
#include <pspkernel.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspwlan.h>
#include <psputility_sysparam.h>
#include <stdio.h>
#include <string.h>
#include "netlink.h"
#include "system.h"

#define MODULE_ALREADY_LOADED 0x80111102 /* the answer of a load that was done before (wifi.c has the same) */
#define JOIN_MS 60000                    /* a connection that is not made in a minute has failed */
/* The network libraries are started with the numbers of ARK's own launcher (arkMenu, its FTP server and its update check): a pool of 256 KB, the two net threads at
 * priority 42 and the access point control with 10 KB of stack and priority 48. */
#define NET_POOL  (256 * 1024)
#define NET_PRIO  42
#define APCTL_STACK (10 * 1024)
#define APCTL_PRIO  48

static int s_common = -1, s_inet_mod = -1, s_net = -1, s_inet = -1, s_apctl = -1, s_handler = -1; /* what was started (>= 0) */
static int s_connecting;                                                         /* netlink_attach() was called */
static unsigned s_since;
static int s_fresh;                                                              /* the clock has not been read since netlink_attach() */
static int s_up;
static char s_ip[16];
static char s_last_ip[16];
static int s_last_loaded;

/* arkMenu registers a handler for the events of the access point control too (it does nothing with them): the connection is watched by asking its state */
static void apctl_event(int prev_state, int new_state, int event, int error, void *arg)
{
}

int netlink_switch_on(void)
{
    return sceWlanGetSwitchState() > 0;
}

int netlink_power_save(void)
{
    int v = 0;
    return sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE, &v) >= 0 && v == PSP_SYSTEMPARAM_WLAN_POWERSAVE_ON;
}

int netlink_start(void)
{
    if (s_apctl >= 0)
        return 1;
    s_common = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    s_inet_mod = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (!(s_common >= 0 || s_common == (int)MODULE_ALREADY_LOADED) || !(s_inet_mod >= 0 || s_inet_mod == (int)MODULE_ALREADY_LOADED))
        return 0;
    if ((s_net = sceNetInit(NET_POOL, NET_PRIO, 0, NET_PRIO, 0)) < 0 || (s_inet = sceNetInetInit()) < 0 || (s_apctl = sceNetApctlInit(APCTL_STACK, APCTL_PRIO)) < 0)
        return 0;
    s_handler = sceNetApctlAddHandler(apctl_event, NULL); /* (a failure is not one: nothing depends on it) */
    return 1;
}

void netlink_attach(void)
{
    if (s_apctl < 0)
        return;
    s_connecting = 1;
    s_up = 0;
    s_fresh = 1; /* (the minute is counted from the first question: the window may have been open for a long time) */
    s_ip[0] = 0;
}

/* "a.b.c.d" of the PSP's address as the connection gave it; 0 when it is not an address. */
static unsigned parse_ip(const char *s)
{
    unsigned a, b, c, d;
    char tail;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255 || !(a | b | c | d))
        return 0;
    return (a << 24) | (b << 16) | (c << 8) | d;
}

const char *netlink_last_ip(void)
{
    if (!s_last_loaded) {
        char path[64], buf[17] = "";
        s_last_loaded = 1;
        snprintf(path, sizeof(path), "%s/lastip.txt", g_data_dir);
        FILE *f = fopen(path, "rb");
        if (f) {
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            int ok = !ferror(f) && n < sizeof(buf) - 1 && strlen(buf) == n;
            fclose(f);
            if (ok && parse_ip(buf))
                snprintf(s_last_ip, sizeof(s_last_ip), "%s", buf);
        }
    }
    return s_last_ip;
}

static void remember_ip(const char *ip)
{
    if (!strcmp(netlink_last_ip(), ip))
        return;
    snprintf(s_last_ip, sizeof(s_last_ip), "%s", ip);
    char path[64];
    snprintf(path, sizeof(path), "%s/lastip.txt", g_data_dir);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(s_last_ip, 1, strlen(s_last_ip), f);
        fclose(f);
    }
}

int netlink_state(unsigned now_ms)
{
    int st = 0;
    if (!s_connecting || sceNetApctlGetState(&st) < 0)
        return LINK_FAILED;
    if (s_fresh) {
        s_since = now_ms;
        s_fresh = 0;
    }
    if (st == PSP_NET_APCTL_STATE_GOT_IP) {
        SceNetApctlInfo info;
        memset(&info, 0, sizeof info);
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) >= 0 && memchr(info.ip, 0, sizeof(info.ip)) && parse_ip(info.ip)) {
            snprintf(s_ip, sizeof s_ip, "%s", info.ip);
            remember_ip(s_ip);
            s_up = 1;
            return LINK_UP;
        }
        return now_ms - s_since > JOIN_MS ? LINK_FAILED : LINK_CONNECTING; /* (the address is not there yet) */
    }
    if (s_up || st == PSP_NET_APCTL_STATE_DISCONNECTED) { /* it was up and is gone, or it is not there */
        s_up = 0;
        s_ip[0] = 0;
        return LINK_FAILED;
    }
    return now_ms - s_since > JOIN_MS ? LINK_FAILED : LINK_CONNECTING; /* joining */
}

const char *netlink_ip(void)
{
    return s_ip;
}

unsigned netlink_ip_value(void)
{
    return s_ip[0] ? parse_ip(s_ip) : 0;
}

static int current_ip(char out[16])
{
    int st = 0;
    if (s_apctl < 0 || !netlink_switch_on() || sceNetApctlGetState(&st) < 0 || st != PSP_NET_APCTL_STATE_GOT_IP)
        return 0;
    SceNetApctlInfo info;
    memset(&info, 0, sizeof info);
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) < 0)
        return 0;
    if (!memchr(info.ip, 0, sizeof(info.ip)))
        return 0;
    if (!parse_ip(info.ip))
        return 0;
    snprintf(out, 16, "%s", info.ip);
    return 1;
}

void netlink_stop(void)
{
    char ip[16];
    if (current_ip(ip)) /* the Wi-Fi dialog can connect without netlink_attach()/netlink_state() */
        remember_ip(ip);
    if (s_connecting)
        sceNetApctlDisconnect();
    s_connecting = 0;
    s_up = 0;
    s_ip[0] = 0;
    if (s_handler >= 0)
        sceNetApctlDelHandler(s_handler);
    if (s_apctl >= 0)
        sceNetApctlTerm();
    if (s_inet >= 0)
        sceNetInetTerm();
    if (s_net >= 0)
        sceNetTerm();
    if (s_inet_mod >= 0)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    if (s_common >= 0)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    s_common = s_inet_mod = s_net = s_inet = s_apctl = s_handler = -1;
}
