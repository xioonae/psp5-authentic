/* psp5-authentic – the screen of the FTP server (Settings → Network → FTP Server) */
#ifndef FTPSCREEN_H
#define FTPSCREEN_H

#include "ui.h"

/* The screen, in the style of USB Mode: the PSP's own Wi-Fi window opens (the one games show; the player chooses a saved connection there and the system connects),
 * then the FTP server (ftp.c) starts and the screen shows the address to type on the computer, whether a computer is connected and the file that is going. ○ stops it. When
 * there is no connection (the window was closed, or something failed) the screen says so and ✕ opens the window again.
 * main.c runs it in its own loop (ftp_mode): it asks for the buttons, calls ftpscreen_update() and ftpscreen_draw() every frame and ftpscreen_close() when it is over. */

typedef enum {
    FTPS_START,      /* the first frames, before the PSP's window opens (it takes a moment) */
    FTPS_SWITCH_OFF, /* the window did not give a connection and the Wi-Fi switch of the PSP is off (waits until it is on, then opens the window again) */
    FTPS_STOPPED,    /* the window was closed without a connection (the switch is on): "not running", ✕ opens the window again */
    FTPS_CONNECTING, /* the window has made the connection: waiting for the address */
    FTPS_RUNNING,    /* the server runs */
    FTPS_FAILED,     /* a message (the network could not be started, the connection failed or was lost, the server could not start); ✕ opens the window again */
} FtpScreenPhase;

/* Before the screen is shown. `running` = main.c's flag that becomes 0 when the PSP is to quit (HOME): the PSP's window ends when it does. */
void ftpscreen_open(volatile int *running);
/* Every frame, with the buttons (the screen is not asked for the power popup's buttons: main.c clears them while it is open). `now_ms` = a clock in milliseconds.
 * Returns 1 when the screen is to close (○). */
int ftpscreen_update(const UiInput *in, float dt, unsigned now_ms);
/* The server once more (only while it runs): the loop calls it a few times between two frames, so that a computer's next command does not wait for the next picture. */
void ftpscreen_poll(unsigned now_ms);
/* The whole frame except the popup of the power menu and the volume bar (power_draw, osd_draw: main.c draws them after it, then gfx_end). */
void ftpscreen_draw(float t);
/* Stops the server and the network (also when it never ran). */
void ftpscreen_close(void);

/* Which phase it is in (for the tests). */
FtpScreenPhase ftpscreen_phase(void);

#endif
