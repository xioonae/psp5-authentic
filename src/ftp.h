/* psp5-authentic – the FTP server: copy files from and to a computer over Wi-Fi */
#ifndef FTP_H
#define FTP_H

/* A small FTP server for the Memory Stick (and the internal storage of a PSP Go). The computer sees two folders at the top, `ms0` and (on a PSP Go) `ef0`; nothing
 * else of the PSP can be reached (not flash0, not flash1). Any user name and password are accepted. Passive mode only (PASV and EPSV).
 * It runs inside the screen's own loop, no thread: ftp_step() is called every frame, never waits, and does a little of every job each time.
 * The network must be up (netlink.c); the sockets are the ones of netsock.h. */

#define FTP_PORT 21

/* What the screen shows. */
enum { FTP_IDLE, FTP_SENDING, FTP_RECEIVING }; /* sending = the computer downloads a file from the PSP */
typedef struct {
    int clients;                    /* computers that are logged in or connected */
    int activity;                   /* FTP_IDLE, FTP_SENDING or FTP_RECEIVING (a file; a folder listing is too quick to show) */
    char name[64];                  /* the name of that file (the end of it) */
    unsigned long long done, total; /* bytes of it, and its size (0 = not known: a file that is received) */
} FtpStatus;

/* Starts listening on `port` (FTP_PORT). `fallback_ip` = the address of the PSP (a << 24 | b << 16 | c << 8 | d) for the answer to PASV, only used when the
 * connection itself does not tell it (0 = none). Returns 1 when it is listening, 0 when not (the port is taken, no sockets). */
int ftp_start(unsigned short port, unsigned fallback_ip);

/* Every frame. `now_ms` = a clock in milliseconds (it may wrap). */
void ftp_step(unsigned now_ms);

/* Closes everything: the computers, the transfers (a half-received file stays as it is), the port. Also when it never started. */
void ftp_stop(void);

/* The port it listens on (0 = not started). */
unsigned short ftp_port(void);

void ftp_status(FtpStatus *out);

#endif
