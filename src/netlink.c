/* psp5-authentic – the connection to a Wi-Fi network (see netlink.h): the network libraries are started the way a game does it, and the access point is joined with
 * sceNetApctlConnect (the calls of the PSP's own connection menu, without its screen). */
#include <pspkernel.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspwlan.h>
#include <stdio.h>
#include <string.h>
#include "netlink.h"

#define MODULE_ALREADY_LOADED 0x80111102 /* the answer of a load that was done before (wifi.c has the same) */
#define JOIN_MS 60000                    /* a connection that is not made in a minute has failed */
#define START_MS 8000                    /* the state of the connection stays "none" for a moment after sceNetApctlConnect */
#define LAST_ID 100                      /* the connections are numbered from 1 (there can be gaps); this many are looked at */

static int s_common = -1, s_inet_mod = -1, s_net = -1, s_inet = -1, s_apctl = -1; /* what was started (>= 0) */
static int s_connecting;                                                         /* sceNetApctlConnect was called */
static unsigned s_since;
static int s_seen;                                                               /* the state has been something else than "none" */
static int s_up;
static char s_ip[16];

int netlink_switch_on(void)
{
    return sceWlanGetSwitchState() > 0;
}

int netlink_start(void)
{
    if (s_apctl >= 0)
        return 1;
    s_common = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    s_inet_mod = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (!(s_common >= 0 || s_common == (int)MODULE_ALREADY_LOADED) || !(s_inet_mod >= 0 || s_inet_mod == (int)MODULE_ALREADY_LOADED))
        return 0;
    if ((s_net = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000)) < 0 || (s_inet = sceNetInetInit()) < 0 || (s_apctl = sceNetApctlInit(0x1600, 42)) < 0)
        return 0;
    return 1;
}

int netlink_connections(NetConn *out, int max)
{
    int n = 0;
    for (int id = 1; id <= LAST_ID && n < max; id++) {
        netData d;
        if (sceUtilityCheckNetParam(id) != 0)
            continue;
        memset(&d, 0, sizeof d);
        if (sceUtilityGetNetParam(id, PSP_NETPARAM_NAME, &d) != 0)
            continue;
        out[n].id = id;
        snprintf(out[n].name, sizeof out[n].name, "%s", d.asString);
        if (!out[n].name[0])
            snprintf(out[n].name, sizeof out[n].name, "%d", id);
        n++;
    }
    return n;
}

int netlink_connect(int id, unsigned now_ms)
{
    if (s_apctl < 0)
        return 0;
    s_up = 0;
    s_seen = 0;
    s_ip[0] = 0;
    s_since = now_ms;
    s_connecting = sceNetApctlConnect(id) >= 0;
    return s_connecting;
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

int netlink_state(unsigned now_ms)
{
    int st = 0;
    if (!s_connecting || sceNetApctlGetState(&st) < 0)
        return LINK_FAILED;
    if (st == PSP_NET_APCTL_STATE_GOT_IP) {
        SceNetApctlInfo info;
        memset(&info, 0, sizeof info);
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) >= 0 && parse_ip(info.ip)) {
            snprintf(s_ip, sizeof s_ip, "%s", info.ip);
            s_up = 1;
            s_seen = 1;
            return LINK_UP;
        }
        return now_ms - s_since > JOIN_MS ? LINK_FAILED : LINK_CONNECTING; /* (the address is not there yet) */
    }
    if (s_up) { /* it was up: the connection is lost */
        s_up = 0;
        s_ip[0] = 0;
        return LINK_FAILED;
    }
    if (st != PSP_NET_APCTL_STATE_DISCONNECTED) {
        s_seen = 1;
        return now_ms - s_since > JOIN_MS ? LINK_FAILED : LINK_CONNECTING;
    }
    if (s_seen || now_ms - s_since > START_MS) /* it began and is "none" again, or it never began */
        return LINK_FAILED;
    return LINK_CONNECTING;
}

const char *netlink_ip(void)
{
    return s_ip;
}

unsigned netlink_ip_value(void)
{
    return s_ip[0] ? parse_ip(s_ip) : 0;
}

void netlink_stop(void)
{
    if (s_connecting)
        sceNetApctlDisconnect();
    s_connecting = 0;
    s_up = 0;
    s_seen = 0;
    s_ip[0] = 0;
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
    s_common = s_inet_mod = s_net = s_inet = s_apctl = -1;
}
