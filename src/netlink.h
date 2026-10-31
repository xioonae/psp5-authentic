/* psp5-authentic – the connection to a Wi-Fi network (for the FTP server) */
#ifndef NETLINK_H
#define NETLINK_H

/* Starts and stops the network libraries and watches the connection that the PSP's own Wi-Fi window (wifi.c, wifi_connect) has made: the player chooses the saved
 * connection there, the system connects. Nothing here waits; the screen asks every frame (ftpscreen.c). Everything that was started is stopped again by netlink_stop(),
 * so the PSP is not left connected. */

enum { LINK_CONNECTING, LINK_UP, LINK_FAILED };

/* 1 = the Wi-Fi switch of the PSP is on. */
int netlink_switch_on(void);

/* Starts the network libraries. 1 = it is done (or was), 0 = it could not be (no memory, the modules would not load). Call netlink_stop() in any case afterwards. */
int netlink_start(void);

/* The libraries are started and the system's window has connected the PSP: from now on netlink_state() watches that connection. */
void netlink_attach(void);

/* LINK_CONNECTING while the connection is not there yet (a minute at most), LINK_UP when the PSP has its address (netlink_ip()), LINK_FAILED when there is no connection
 * or when it was lost afterwards. `now_ms` = a clock in milliseconds. */
int netlink_state(unsigned now_ms);

/* The address of the PSP, "192.168.1.23" ("" while there is none). */
const char *netlink_ip(void);
/* The same as a number: a << 24 | b << 16 | c << 8 | d (0 = none). */
unsigned netlink_ip_value(void);

/* Disconnects and stops the network libraries. Also when nothing was started. */
void netlink_stop(void);

#endif
