/* psp5-authentic – the connection to a Wi-Fi network (for the FTP server) */
#ifndef NETLINK_H
#define NETLINK_H

/* Connects the PSP to one of its saved Wi-Fi connections (the ones made in the XMB: Settings > Network Settings > Infrastructure Mode), without any screen of the system's:
 * psp5 shows the list and the progress itself (ftpscreen.c). Nothing here waits; the screen asks every frame. Everything that was started is stopped again by
 * netlink_stop(), so the PSP is not left connected. */

#define NETLINK_MAX 32 /* the most connections that are listed */

typedef struct {
    int id;        /* the number of the connection (1 ...) */
    char name[48]; /* its name, as it was saved */
} NetConn;

enum { LINK_CONNECTING, LINK_UP, LINK_FAILED };

/* 1 = the Wi-Fi switch of the PSP is on. */
int netlink_switch_on(void);

/* Starts the network libraries. 1 = it is done (or was), 0 = it could not be (no memory, the modules would not load). Call netlink_stop() in any case afterwards. */
int netlink_start(void);

/* The saved connections, in the PSP's order; returns how many (at most `max`). The libraries must be started. */
int netlink_connections(NetConn *out, int max);

/* Starts connecting to the connection `id`. 1 = it was started (ask netlink_state() every frame), 0 = no. `now_ms` = a clock in milliseconds. */
int netlink_connect(int id, unsigned now_ms);

/* LINK_CONNECTING while it joins and asks for an address (a minute at most), LINK_UP when the PSP has its address (netlink_ip()), LINK_FAILED when it could not connect or when
 * the connection was lost afterwards. */
int netlink_state(unsigned now_ms);

/* The address of the PSP, "192.168.1.23" ("" while there is none). */
const char *netlink_ip(void);
/* The same as a number: a << 24 | b << 16 | c << 8 | d (0 = none). */
unsigned netlink_ip_value(void);

/* Disconnects and stops the network libraries. Also when nothing was started. */
void netlink_stop(void);

#endif
