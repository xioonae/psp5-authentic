/* psp5-authentic – the FTP server (see ftp.h) */
#include <pspkernel.h>
#include <psprtc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ftp.h"
#include "netsock.h"

#define MAX_SESSIONS 3        /* computers at the same time (a few clients open two connections) */
#define LINE_MAX_LEN 512      /* one command */
#define OUT_MAX 1024          /* the answers that have not gone yet (one command is answered before the next is read) */
#define CHUNK 16384           /* one read or write of a file */
#define PATH_LEN 256
#define MAX_DEPTH 48          /* folders in a path */
#define IDLE_MS (5 * 60 * 1000)
#define DATA_WAIT_MS 20000    /* how long a transfer waits for the computer to open its data connection */
#define STALL_MS 60000        /* a transfer that moves no byte for this long is ended (the computer has gone, or does not read) */
#define PASV_FIRST 49152      /* the ports of the data connections: 1000 of them, one after the other */
#define PASV_COUNT 1000
#define STEPS 4               /* chunks per transfer and frame (the screen must stay alive) */

typedef enum { X_NONE, X_LIST, X_NLST, X_RETR, X_STOR } Xfer;

typedef struct {
    int used;                 /* this slot has a computer (a static slot is 0 before the server started) */
    int ctl;                  /* control connection */
    int logged;               /* USER and PASS are done */
    int user_seen;
    int quit;                 /* QUIT: close as soon as the answer has gone */
    int skip;                 /* the rest of a line that was too long is thrown away */
    char in[LINE_MAX_LEN];
    int in_len;
    char out[OUT_MAX];
    int out_len, out_pos;
    char cwd[PATH_LEN];       /* "/" or "/ms0/PSP" */
    char rnfr[PATH_LEN];      /* the path that RNFR chose, "" = none */
    unsigned rest;            /* REST: where the next RETR / STOR starts */
    int pasv;                 /* the socket that waits for the data connection, -1 = none */
    unsigned last_active;
    /* the transfer */
    Xfer kind;
    int ds;                   /* the data connection, -1 while it is waited for */
    unsigned wait_since;
    SceUID fd;                /* the file (RETR, STOR) or the folder (LIST, NLST) */
    int is_root;              /* a list of "/": the two devices, not a folder */
    int listed;               /* how many of the root's entries are done (LIST of "/") */
    int list_done;
    char buf[CHUNK];
    int blen, bpos;
    unsigned long long done, total;
    char name[64];
} Session;

static Session s_sess[MAX_SESSIONS];
static int s_listen = -1;
static unsigned short s_port;
static unsigned s_fallback_ip;
static unsigned s_now;
static unsigned s_pasv_next;

static const char *const MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* ---- answers ---------------------------------------------------------------------------------------------------------------------------------------------- */

/* Queues an answer (the line end is added). An answer of several lines has its own \r\n inside. */
static void reply(Session *s, const char *fmt, ...)
{
    int room = OUT_MAX - s->out_len;
    if (room < 8)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s->out + s->out_len, room - 2, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > room - 3)
        n = room - 3;
    s->out_len += n;
    s->out[s->out_len++] = '\r';
    s->out[s->out_len++] = '\n';
}

/* ---- paths ------------------------------------------------------------------------------------------------------------------------------------------------- */

/* A name that a FAT storage can have (and that cannot lead anywhere else). `len` is never 0. */
static int name_ok(const char *p, int len)
{
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c < 0x20 || c == 0x7F || strchr("\\:*?\"<>|", c))
            return 0;
    }
    return 1;
}

/* The path `arg` as seen from the folder `cwd`, in the computer's view: "/ms0/PSP/GAME". "." and ".." are resolved (".." at the top stays there). The first name
 * is written in small letters when it is ms0 or ef0. Returns 0, or -1 for a path that is too long or has a name that cannot exist. */
static int vresolve(const char *cwd, const char *arg, char *out, int n)
{
    char tmp[2 * PATH_LEN + 2];
    if (strlen(arg) >= PATH_LEN)
        return -1;
    if (arg[0] == '/')
        snprintf(tmp, sizeof tmp, "%s", arg);
    else
        snprintf(tmp, sizeof tmp, "%s/%s", cwd, arg);
    const char *comp[MAX_DEPTH];
    int clen[MAX_DEPTH], depth = 0;
    const char *p = tmp;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        const char *e = p;
        while (*e && *e != '/')
            e++;
        int len = (int)(e - p);
        if (len == 1 && p[0] == '.') {
            /* stays */
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            if (depth)
                depth--;
        } else {
            if (depth >= MAX_DEPTH || !name_ok(p, len)) /* (a name cannot be longer than the path, and that is shorter than PATH_LEN) */
                return -1;
            comp[depth] = p;
            clen[depth++] = len;
        }
        p = e;
    }
    if (!depth) {
        snprintf(out, n, "/");
        return 0;
    }
    int o = 0;
    for (int i = 0; i < depth; i++) {
        int w = snprintf(out + o, n - o, "/%.*s", clen[i], comp[i]);
        if (w < 0 || w >= n - o)
            return -1;
        o += w;
    }
    if (clen[0] == 3 && (!strncasecmp(comp[0], "ms0", 3) || !strncasecmp(comp[0], "ef0", 3))) {
        for (int i = 1; i <= 3; i++)
            if (out[i] >= 'A' && out[i] <= 'Z')
                out[i] += 'a' - 'A';
    }
    return 0;
}

static int dir_exists(const char *real)
{
    SceUID d = sceIoDopen(real);
    if (d < 0)
        return 0;
    sceIoDclose(d);
    return 1;
}

/* 1 = the folder can be opened and has nothing in it. RMD checks this itself: the PSP's remove of a folder is not trusted to refuse one that is not empty (PPSSPP deletes it
 * with everything in it), and a computer that asks for RMD expects the answer 550 and its files untouched. */
static int dir_empty(const char *real)
{
    SceUID d = sceIoDopen(real);
    if (d < 0)
        return 0;
    int empty = 1;
    for (;;) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof ent);
        if (sceIoDread(d, &ent) <= 0)
            break;
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..")) {
            empty = 0;
            break;
        }
    }
    sceIoDclose(d);
    return empty;
}

/* The place on the PSP of a path of the computer's view: 0 = `real` is it ("ms0:/PSP"), 1 = it is the top "/" (no place), -1 = not something that is served (only ms0 and ef0
 * are: never flash0 or flash1; on a PSP that has no ef0 every call on it fails by itself). */
static int real_path(const char *v, char *real, int n)
{
    if (!strcmp(v, "/"))
        return 1;
    const char *dev = v + 1;
    int dl = (int)strcspn(dev, "/");
    if (dl != 3 || (strncmp(dev, "ms0", 3) && strncmp(dev, "ef0", 3)))
        return -1;
    const char *rest = dev + dl;
    int w = snprintf(real, n, "%.3s:%s", dev, *rest ? rest : "/");
    if (w < 0 || w >= n)
        return -1;
    return 0;
}

/* 1 = this is a device itself ("/ms0"): its folders can be made and removed inside it, but it cannot be written to, removed or renamed. */
static int is_device(const char *v)
{
    return strlen(v) == 4 && v[0] == '/';
}

static const char *base_name(const char *v)
{
    const char *b = strrchr(v, '/');
    return b ? b + 1 : v;
}

/* ---- the data connection and transfers ----------------------------------------------------------------------------------------------------------------- */

static void close_pasv(Session *s)
{
    if (s->pasv >= 0)
        netsock_close(s->pasv);
    s->pasv = -1;
}

/* Everything of a transfer is closed (the sockets, the file or folder). */
static void xfer_close(Session *s)
{
    if (s->ds >= 0)
        netsock_close(s->ds);
    s->ds = -1;
    close_pasv(s);
    if (s->fd >= 0) {
        if (s->kind == X_LIST || s->kind == X_NLST)
            sceIoDclose(s->fd);
        else
            sceIoClose(s->fd);
    }
    s->fd = -1;
    s->kind = X_NONE;
    s->blen = s->bpos = 0;
    s->done = s->total = 0;
    s->name[0] = 0;
    s->rest = 0;
}

/* The transfer is over: 226, or the error answer `err` (with its number). */
static void xfer_end(Session *s, const char *err)
{
    xfer_close(s);
    if (err)
        reply(s, "%s", err);
    else
        reply(s, "226 Transfer complete");
}

static void format_entry(Session *s, int is_dir, unsigned long long size, const ScePspDateTime *t, const char *name, int names_only)
{
    char line[PATH_LEN + 96];
    int n;
    if (names_only) {
        n = snprintf(line, sizeof line, "%s\r\n", name);
    } else {
        ScePspDateTime now;
        memset(&now, 0, sizeof now);
        sceRtcGetCurrentClockLocalTime(&now);
        int ok = t->year >= 1980 && t->month >= 1 && t->month <= 12 && t->day >= 1 && t->day <= 31; /* (a time that is not known is the first day of FAT) */
        int year = ok ? t->year : 1980, mon = ok ? t->month : 1, day = ok ? t->day : 1;
        if (size > 0xFFFFFFFFull)
            size = 0xFFFFFFFFull;
        if (ok && year == now.year)
            n = snprintf(line, sizeof line, "%s 1 psp psp %10u %s %2d %02d:%02d %s\r\n", is_dir ? "drwxr-xr-x" : "-rw-r--r--", (unsigned)size, MONTHS[mon - 1], day, t->hour % 24, t->minute % 60, name);
        else
            n = snprintf(line, sizeof line, "%s 1 psp psp %10u %s %2d  %04d %s\r\n", is_dir ? "drwxr-xr-x" : "-rw-r--r--", (unsigned)size, MONTHS[mon - 1], day, year, name);
    }
    if (n < 0 || n >= (int)sizeof line)
        return;
    if (s->blen + n <= CHUNK) {
        memcpy(s->buf + s->blen, line, n);
        s->blen += n;
    }
}

/* Writes the next entries of the list into the buffer (the buffer is empty when it is called). */
static void list_fill(Session *s)
{
    int names_only = s->kind == X_NLST;
    s->blen = s->bpos = 0;
    if (s->is_root) {
        static const char *const dev[2] = { "ms0", "ef0" };
        while (s->listed < 2 && s->blen < CHUNK - 400) {
            const char *d = dev[s->listed++];
            ScePspDateTime t;
            memset(&t, 0, sizeof t);
            sceRtcGetCurrentClockLocalTime(&t);
            if (!strcmp(d, "ms0") || dir_exists("ef0:/"))
                format_entry(s, 1, 0, &t, d, names_only);
        }
        if (s->listed >= 2)
            s->list_done = 1;
        return;
    }
    while (s->blen < CHUNK - 400) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof ent);
        if (sceIoDread(s->fd, &ent) <= 0) {
            s->list_done = 1;
            return;
        }
        if (!strcmp(ent.d_name, ".") || !strcmp(ent.d_name, ".."))
            continue;
        format_entry(s, FIO_S_ISDIR(ent.d_stat.st_mode) != 0, (unsigned long long)ent.d_stat.st_size, &ent.d_stat.sce_st_mtime, ent.d_name, names_only);
    }
}

/* Sends what is in the buffer. 1 = all of it is gone (the buffer is empty), 0 = the rest waits for the next frame, -1 = the connection is broken. */
static int flush_buf(Session *s)
{
    if (s->bpos >= s->blen)
        return 1;
    int n = netsock_send(s->ds, s->buf + s->bpos, s->blen - s->bpos);
    if (n < 0)
        return -1;
    s->bpos += n;
    s->done += (unsigned)n;
    if (s->bpos < s->blen)
        return 0;
    s->bpos = s->blen = 0;
    return 1;
}

static void xfer_step(Session *s)
{
    if (s->ds < 0) { /* the computer has not opened its data connection yet */
        int c = s->pasv >= 0 ? netsock_accept(s->pasv) : -1;
        if (c >= 0) {
            s->ds = c;
            close_pasv(s);
            s->last_active = s_now;
        } else if (s_now - s->wait_since > DATA_WAIT_MS) {
            xfer_end(s, "425 Can't open the data connection");
        }
        return;
    }
    if (s_now - s->last_active > STALL_MS) {
        xfer_end(s, "426 The connection stopped");
        return;
    }
    for (int i = 0; i < STEPS; i++) {
        if (s->kind == X_STOR) {
            int n = netsock_recv(s->ds, s->buf, CHUNK);
            if (n == NETSOCK_AGAIN)
                return;
            if (n < 0) {
                xfer_end(s, "426 Connection closed; transfer aborted");
                return;
            }
            if (n == 0) { /* the computer has sent it all and closed */
                xfer_end(s, NULL);
                return;
            }
            if (sceIoWrite(s->fd, s->buf, n) != n) {
                xfer_end(s, "552 The file could not be written (is the storage full?)");
                return;
            }
            s->done += (unsigned)n;
            s->last_active = s_now;
            continue;
        }
        if (s->bpos >= s->blen) { /* the buffer is empty: fill it */
            if (s->kind == X_RETR) {
                int n = sceIoRead(s->fd, s->buf, CHUNK);
                if (n < 0) {
                    xfer_end(s, "451 The file could not be read");
                    return;
                }
                if (n == 0) {
                    xfer_end(s, NULL);
                    return;
                }
                s->blen = n;
                s->bpos = 0;
            } else {
                if (s->list_done) {
                    xfer_end(s, NULL);
                    return;
                }
                list_fill(s);
                continue;
            }
        }
        unsigned long long before = s->done;
        int r = flush_buf(s);
        if (r < 0) {
            xfer_end(s, "426 Connection closed; transfer aborted");
            return;
        }
        if (s->done != before)
            s->last_active = s_now;
        if (r == 0)
            return;
    }
}

/* Waits for the data connection and starts the transfer `kind` (the file or folder `fd` is open). The first answer goes now, the last one when it is over. */
static void xfer_begin(Session *s, Xfer kind, SceUID fd, unsigned long long total, const char *name)
{
    s->kind = kind;
    s->fd = fd;
    s->ds = -1;
    s->wait_since = s_now;
    s->blen = s->bpos = 0;
    s->done = 0;
    s->total = total;
    snprintf(s->name, sizeof s->name, "%s", name);
    s->last_active = s_now;
    reply(s, "150 Opening the data connection");
}

/* ---- commands ----------------------------------------------------------------------------------------------------------------------------------------------- */

/* Which place the argument means: the computer's path in `v`, the PSP's in `real`. Returns real_path()'s answer, or -1 (the answer 550 is queued by the caller). */
static int where(Session *s, const char *arg, char *v, char *real, int n)
{
    if (vresolve(s->cwd, arg, v, PATH_LEN) < 0)
        return -1;
    return real_path(v, real, n);
}

static void cmd_user(Session *s, char *arg)
{
    s->user_seen = 1;
    s->logged = 0;
    reply(s, "331 Any password will do");
}

static void cmd_pass(Session *s, char *arg)
{
    if (!s->user_seen) {
        reply(s, "503 Send USER first");
        return;
    }
    s->logged = 1;
    reply(s, "230 Logged in");
}

static void cmd_quit(Session *s, char *arg)
{
    s->quit = 1;
    reply(s, "221 Goodbye");
}

static void cmd_noop(Session *s, char *arg) { reply(s, "200 OK"); }
static void cmd_syst(Session *s, char *arg) { reply(s, "215 UNIX Type: L8"); }
static void cmd_allo(Session *s, char *arg) { reply(s, "202 Not needed"); }

static void cmd_feat(Session *s, char *arg)
{
    reply(s, "211-Features:\r\n SIZE\r\n MDTM\r\n REST STREAM\r\n UTF8\r\n EPSV\r\n211 End");
}

static void cmd_opts(Session *s, char *arg)
{
    if (!strncasecmp(arg, "UTF8", 4))
        reply(s, "200 UTF-8 is used");
    else
        reply(s, "501 Option not supported");
}

static void cmd_type(Session *s, char *arg)
{
    if (*arg == 'A' || *arg == 'a' || *arg == 'I' || *arg == 'i' || *arg == 'L' || *arg == 'l')
        reply(s, "200 Type set"); /* (the bytes of a file are never changed) */
    else
        reply(s, "504 Type not supported");
}

static void cmd_mode(Session *s, char *arg)
{
    reply(s, (*arg == 'S' || *arg == 's') ? "200 Stream mode" : "504 Only stream mode");
}

static void cmd_stru(Session *s, char *arg)
{
    reply(s, (*arg == 'F' || *arg == 'f') ? "200 File structure" : "504 Only file structure");
}

static void cmd_pwd(Session *s, char *arg)
{
    reply(s, "257 \"%s\" is the current folder", s->cwd); /* (no name has a quote in it: name_ok) */
}

static void cmd_cwd(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    int w = where(s, arg, v, real, sizeof real);
    if (w == 1 || (w == 0 && dir_exists(real))) {
        snprintf(s->cwd, sizeof s->cwd, "%s", v);
        reply(s, "250 OK");
    } else {
        reply(s, "550 No such folder");
    }
}

static void cmd_cdup(Session *s, char *arg)
{
    char up[] = "..";
    cmd_cwd(s, up);
}

static void cmd_rest(Session *s, char *arg)
{
    char *end;
    unsigned long v = strtoul(arg, &end, 10);
    if (end == arg || *end || v > 0x7FFFFFFFul) { /* (a position that sceIoLseek32 can reach) */
        reply(s, "501 Bad position");
        return;
    }
    s->rest = (unsigned)v;
    reply(s, "350 Restarting at %lu", v);
}

/* A transfer command that cannot start: the answer, and the data connection that was made for it is not kept (every transfer has its own PASV). */
static void xfer_fail(Session *s, const char *answer)
{
    close_pasv(s);
    s->rest = 0;
    reply(s, "%s", answer);
}

/* The data connection will be waited for on a new port: the socket is made, the port is returned (0 = no socket could be made). */
static unsigned short pasv_open(Session *s)
{
    close_pasv(s);
    for (int i = 0; i < 32; i++) {
        unsigned short p = (unsigned short)(PASV_FIRST + (s_pasv_next++ % PASV_COUNT));
        int ls = netsock_listen(p);
        if (ls >= 0) {
            s->pasv = ls;
            return p;
        }
    }
    return 0;
}

static void cmd_pasv(Session *s, char *arg)
{
    unsigned ip = netsock_local_ip(s->ctl);
    if (!ip)
        ip = s_fallback_ip;
    unsigned short p = ip ? pasv_open(s) : 0;
    if (!p) {
        reply(s, "425 Can't open a data connection");
        return;
    }
    reply(s, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, p >> 8, p & 255);
}

static void cmd_epsv(Session *s, char *arg)
{
    if (*arg && strcasecmp(arg, "ALL") && strcmp(arg, "1")) { /* only IPv4 */
        reply(s, "522 Only IPv4 (1)");
        return;
    }
    unsigned short p = pasv_open(s);
    if (!p) {
        reply(s, "425 Can't open a data connection");
        return;
    }
    reply(s, "229 Entering Extended Passive Mode (|||%u|)", (unsigned)p);
}

static void cmd_port(Session *s, char *arg)
{
    reply(s, "502 Use passive mode (PASV)");
}

/* LIST and NLST: the options of `ls` (-a, -l) are not looked at. */
static void list_cmd(Session *s, char *arg, Xfer kind)
{
    while (*arg == '-') { /* "-a", "-la" ... */
        while (*arg && *arg != ' ')
            arg++;
        while (*arg == ' ')
            arg++;
    }
    char v[PATH_LEN], real[PATH_LEN];
    int w = where(s, *arg ? arg : ".", v, real, sizeof real);
    if (s->pasv < 0) {
        reply(s, "425 Use PASV first");
        return;
    }
    s->rest = 0;
    if (w == 1) {
        s->is_root = 1;
        s->listed = 0;
        s->list_done = 0;
        xfer_begin(s, kind, -1, 0, "/");
        return;
    }
    SceUID d = w == 0 ? sceIoDopen(real) : -1;
    if (d < 0) {
        xfer_fail(s, "550 No such folder");
        return;
    }
    s->is_root = 0;
    s->list_done = 0;
    xfer_begin(s, kind, d, 0, base_name(v));
}

static void cmd_list(Session *s, char *arg) { list_cmd(s, arg, X_LIST); }
static void cmd_nlst(Session *s, char *arg) { list_cmd(s, arg, X_NLST); }

static void cmd_retr(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    int w = where(s, arg, v, real, sizeof real);
    if (s->pasv < 0) {
        reply(s, "425 Use PASV first");
        return;
    }
    if (w != 0 || sceIoGetstat(real, &st) < 0 || FIO_S_ISDIR(st.st_mode)) {
        xfer_fail(s, "550 No such file");
        return;
    }
    unsigned long long size = st.st_size > 0 ? (unsigned long long)st.st_size : 0;
    unsigned rest = s->rest;
    s->rest = 0;
    if (rest > size) {
        xfer_fail(s, "554 Bad position");
        return;
    }
    SceUID fd = sceIoOpen(real, PSP_O_RDONLY, 0);
    if (fd < 0) {
        xfer_fail(s, "550 The file cannot be opened");
        return;
    }
    if (rest && sceIoLseek32(fd, (int)rest, PSP_SEEK_SET) != (int)rest) {
        sceIoClose(fd);
        xfer_fail(s, "554 Bad position");
        return;
    }
    xfer_begin(s, X_RETR, fd, size, base_name(v));
    s->done = rest; /* (the bar starts where the computer starts) */
}

static void store(Session *s, char *arg, int append)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    int w = where(s, arg, v, real, sizeof real);
    if (s->pasv < 0) {
        reply(s, "425 Use PASV first");
        return;
    }
    unsigned rest = s->rest;
    s->rest = 0;
    if (w != 0 || is_device(v)) {
        xfer_fail(s, "550 The file cannot be written here");
        return;
    }
    if (sceIoGetstat(real, &st) >= 0 && FIO_S_ISDIR(st.st_mode)) {
        xfer_fail(s, "550 That is a folder");
        return;
    }
    int keep = append || rest; /* the old content stays */
    SceUID fd = sceIoOpen(real, PSP_O_WRONLY | PSP_O_CREAT | (keep ? 0 : PSP_O_TRUNC), 0777);
    if (fd < 0) {
        xfer_fail(s, "550 The file cannot be written here");
        return;
    }
    if (keep) {
        int size = sceIoLseek32(fd, 0, PSP_SEEK_END);
        int at = append ? size : (int)rest;
        if (size < 0 || at > size || sceIoLseek32(fd, at, PSP_SEEK_SET) != at) { /* (not past the end: that would leave a hole) */
            sceIoClose(fd);
            xfer_fail(s, "554 Bad position");
            return;
        }
    }
    xfer_begin(s, X_STOR, fd, 0, base_name(v));
}

static void cmd_stor(Session *s, char *arg) { store(s, arg, 0); }
static void cmd_appe(Session *s, char *arg) { store(s, arg, 1); }

static void cmd_dele(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || is_device(v) || sceIoGetstat(real, &st) < 0 || FIO_S_ISDIR(st.st_mode) || sceIoRemove(real) < 0)
        reply(s, "550 The file cannot be deleted");
    else
        reply(s, "250 Deleted");
}

static void cmd_mkd(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || is_device(v) || sceIoMkdir(real, 0777) < 0)
        reply(s, "550 The folder cannot be made");
    else
        reply(s, "257 \"%s\" is made", v);
}

static void cmd_rmd(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || is_device(v) || !dir_empty(real) || sceIoRmdir(real) < 0)
        reply(s, "550 The folder cannot be removed (is it empty?)");
    else
        reply(s, "250 Removed");
}

static void cmd_rnfr(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    s->rnfr[0] = 0;
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || is_device(v) || (sceIoGetstat(real, &st) < 0 && !dir_exists(real))) {
        reply(s, "550 No such file or folder");
        return;
    }
    snprintf(s->rnfr, sizeof s->rnfr, "%s", v);
    reply(s, "350 Send the new name with RNTO");
}

static void cmd_rnto(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN], from[PATH_LEN];
    int w = where(s, arg, v, real, sizeof real);
    if (!s->rnfr[0]) {
        reply(s, "503 Send RNFR first");
        return;
    }
    int f = real_path(s->rnfr, from, sizeof from);
    s->rnfr[0] = 0;
    if (w != 0 || f != 0 || is_device(v) || sceIoRename(from, real) < 0)
        reply(s, "550 It could not be renamed");
    else
        reply(s, "250 Renamed");
}

static void cmd_size(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || sceIoGetstat(real, &st) < 0 || FIO_S_ISDIR(st.st_mode))
        reply(s, "550 No such file");
    else
        reply(s, "213 %u", st.st_size > 0xFFFFFFFFll ? 0xFFFFFFFFu : (unsigned)st.st_size);
}

static void cmd_mdtm(Session *s, char *arg)
{
    char v[PATH_LEN], real[PATH_LEN];
    SceIoStat st;
    int w = where(s, arg, v, real, sizeof real);
    if (w != 0 || sceIoGetstat(real, &st) < 0 || FIO_S_ISDIR(st.st_mode)) {
        reply(s, "550 No such file");
        return;
    }
    const ScePspDateTime *t = &st.sce_st_mtime;
    reply(s, "213 %04d%02d%02d%02d%02d%02d", t->year, t->month, t->day, t->hour, t->minute, t->second);
}

static void cmd_abor(Session *s, char *arg) { reply(s, "226 Nothing to stop"); }
static void cmd_auth(Session *s, char *arg) { reply(s, "502 No encryption"); }

typedef struct {
    const char *name;
    void (*fn)(Session *, char *);
    int needs_login;
} Command;

static const Command COMMANDS[] = {
    { "USER", cmd_user, 0 }, { "PASS", cmd_pass, 0 }, { "QUIT", cmd_quit, 0 }, { "NOOP", cmd_noop, 0 }, { "SYST", cmd_syst, 0 }, { "FEAT", cmd_feat, 0 },
    { "OPTS", cmd_opts, 0 }, { "AUTH", cmd_auth, 0 }, { "PBSZ", cmd_auth, 0 }, { "PROT", cmd_auth, 0 }, { "ABOR", cmd_abor, 0 },
    { "TYPE", cmd_type, 1 }, { "MODE", cmd_mode, 1 }, { "STRU", cmd_stru, 1 }, { "ALLO", cmd_allo, 1 },
    { "PWD", cmd_pwd, 1 }, { "XPWD", cmd_pwd, 1 }, { "CWD", cmd_cwd, 1 }, { "XCWD", cmd_cwd, 1 }, { "CDUP", cmd_cdup, 1 }, { "XCUP", cmd_cdup, 1 },
    { "PASV", cmd_pasv, 1 }, { "EPSV", cmd_epsv, 1 }, { "PORT", cmd_port, 1 }, { "EPRT", cmd_port, 1 },
    { "LIST", cmd_list, 1 }, { "NLST", cmd_nlst, 1 }, { "RETR", cmd_retr, 1 }, { "STOR", cmd_stor, 1 }, { "APPE", cmd_appe, 1 }, { "REST", cmd_rest, 1 },
    { "DELE", cmd_dele, 1 }, { "MKD", cmd_mkd, 1 }, { "XMKD", cmd_mkd, 1 }, { "RMD", cmd_rmd, 1 }, { "XRMD", cmd_rmd, 1 },
    { "RNFR", cmd_rnfr, 1 }, { "RNTO", cmd_rnto, 1 }, { "SIZE", cmd_size, 1 }, { "MDTM", cmd_mdtm, 1 },
};

/* One command line (without its end): finds the command and does it. */
static void do_command(Session *s, char *line)
{
    while ((unsigned char)*line >= 0xF0) /* Telnet signs that some clients put in front of ABOR */
        line++;
    char cmd[8];
    int n = 0;
    while (line[n] && line[n] != ' ' && n < (int)sizeof cmd - 1) {
        cmd[n] = line[n] >= 'a' && line[n] <= 'z' ? (char)(line[n] - 32) : line[n];
        n++;
    }
    cmd[n] = 0;
    char *arg = line + n;
    while (*arg && *arg != ' ')
        arg++; /* (a command word that was longer than the table's names: it will not be found) */
    while (*arg == ' ')
        arg++;
    if (!n) {
        reply(s, "500 What?");
        return;
    }
    for (unsigned i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++) {
        if (strcmp(cmd, COMMANDS[i].name))
            continue;
        if (COMMANDS[i].needs_login && !s->logged)
            reply(s, "530 Log in first (USER, PASS)");
        else
            COMMANDS[i].fn(s, arg);
        return;
    }
    reply(s, "500 Unknown command");
}

/* ---- the computers ------------------------------------------------------------------------------------------------------------------------------------- */

static void session_close(Session *s)
{
    if (!s->used)
        return;
    xfer_close(s);
    netsock_close(s->ctl);
    s->ctl = -1;
    s->used = 0;
}

static void session_open(Session *s, int ctl)
{
    memset(s, 0, sizeof *s);
    s->used = 1;
    s->ctl = ctl;
    s->pasv = s->ds = -1;
    s->fd = -1;
    s->last_active = s_now;
    snprintf(s->cwd, sizeof s->cwd, "/");
    reply(s, "220 psp5 authentic FTP server");
}

/* The place in the input of the end of the first line (index of \n), -1 when it is not all there. */
static int line_end(const Session *s)
{
    for (int i = 0; i < s->in_len; i++)
        if (s->in[i] == '\n')
            return i;
    return -1;
}

static void drop_input(Session *s, int n)
{
    memmove(s->in, s->in + n, s->in_len - n);
    s->in_len -= n;
}

/* Sends the answers that wait. Returns 0 when the connection is broken. */
static int flush_out(Session *s)
{
    while (s->out_pos < s->out_len) {
        int n = netsock_send(s->ctl, s->out + s->out_pos, s->out_len - s->out_pos);
        if (n < 0)
            return 0;
        if (n == 0)
            return 1;
        s->out_pos += n;
    }
    s->out_pos = s->out_len = 0;
    return 1;
}

static void session_step(Session *s)
{
    if (!flush_out(s) || (s->quit && s->out_len == 0)) {
        session_close(s);
        return;
    }
    if (s->in_len < LINE_MAX_LEN) {
        int n = netsock_recv(s->ctl, s->in + s->in_len, LINE_MAX_LEN - s->in_len);
        if (n == 0 || n == NETSOCK_ERR) {
            session_close(s);
            return;
        }
        if (n > 0) {
            s->in_len += n;
            s->last_active = s_now;
        }
    }
    if (s->kind != X_NONE && s->out_len == 0)
        xfer_step(s);
    /* the commands, one by one: the next is read when the answer to the one before has gone */
    for (int guard = 0; guard < 8 && s->out_len == 0 && !s->quit; guard++) {
        int e = line_end(s);
        if (e < 0) {
            if (s->skip) { /* the middle of a line that was too long */
                s->in_len = 0;
            } else if (s->in_len >= LINE_MAX_LEN) { /* too long: the rest of the line is thrown away */
                s->in_len = 0;
                s->skip = 1;
                reply(s, "500 The line is too long");
            }
            break;
        }
        if (s->skip) { /* the end of a line that was too long */
            drop_input(s, e + 1);
            s->skip = 0;
            continue;
        }
        char line[LINE_MAX_LEN + 1];
        int len = e;
        if (len && s->in[len - 1] == '\r')
            len--;
        memcpy(line, s->in, len);
        line[len] = 0;
        if (s->kind != X_NONE) { /* a transfer is going: only ABOR and QUIT are taken now, the rest of the commands wait for its end */
            char *p = line;
            while ((unsigned char)*p >= 0xF0)
                p++;
            int abor = !strncasecmp(p, "ABOR", 4), quit = !strncasecmp(p, "QUIT", 4);
            if (!abor && !quit)
                break;
            drop_input(s, e + 1);
            xfer_end(s, "426 Transfer aborted");
            if (abor)
                reply(s, "226 Stopped");
            else
                do_command(s, p);
            continue;
        }
        drop_input(s, e + 1);
        do_command(s, line);
        s->last_active = s_now;
    }
    if (!flush_out(s)) {
        session_close(s);
        return;
    }
    if (s->quit && s->out_len == 0) {
        session_close(s);
        return;
    }
    if (s->kind == X_NONE && s_now - s->last_active > IDLE_MS) { /* nobody has said anything for a long time */
        reply(s, "421 Timeout");
        s->quit = 1;
        flush_out(s);
        session_close(s);
    }
}

/* ---- the server ------------------------------------------------------------------------------------------------------------------------------------------- */

int ftp_start(unsigned short port, unsigned fallback_ip)
{
    ftp_stop();
    s_listen = netsock_listen(port);
    if (s_listen < 0)
        return 0;
    s_port = netsock_port(s_listen);
    if (!s_port)
        s_port = port;
    s_fallback_ip = fallback_ip;
    return 1;
}

void ftp_step(unsigned now_ms)
{
    s_now = now_ms;
    if (s_listen < 0)
        return;
    for (int guard = 0; guard < 4; guard++) {
        int c = netsock_accept(s_listen);
        if (c < 0)
            break;
        Session *slot = NULL;
        for (int i = 0; i < MAX_SESSIONS && !slot; i++)
            if (!s_sess[i].used)
                slot = &s_sess[i];
        if (slot) {
            session_open(slot, c);
        } else {
            netsock_send(c, "421 Too many computers\r\n", 24);
            netsock_close(c);
        }
    }
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (s_sess[i].used)
            session_step(&s_sess[i]);
}

void ftp_stop(void)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        session_close(&s_sess[i]);
    if (s_listen >= 0)
        netsock_close(s_listen);
    s_listen = -1;
    s_port = 0;
}

unsigned short ftp_port(void)
{
    return s_listen >= 0 ? s_port : 0;
}

void ftp_status(FtpStatus *out)
{
    memset(out, 0, sizeof *out);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        const Session *s = &s_sess[i];
        if (!s->used)
            continue;
        out->clients++;
        if ((s->kind == X_RETR || s->kind == X_STOR) && out->activity == FTP_IDLE) {
            out->activity = s->kind == X_RETR ? FTP_SENDING : FTP_RECEIVING;
            snprintf(out->name, sizeof out->name, "%s", s->name);
            out->done = s->done;
            out->total = s->total;
        }
    }
}
