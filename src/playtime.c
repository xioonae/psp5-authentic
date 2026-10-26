/* psp5 – made by Mille Weber */
/* How long each game has been played (see playtime.h). */
#include <pspkernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "playtime.h"
#include "lang.h"

#define KEY_LEN   80   /* as home.c: entry_cover_key() callers have always used 80 */
#define MAX_GAMES 4096 /* (a line is about 90 bytes: a file of at most 370 kB) */
#define MAX_FILE  (MAX_GAMES * (KEY_LEN + 16))
#define MAX_SECS  0xFFFFFF00u

typedef struct {
    char key[KEY_LEN];
    unsigned secs;
} Game;

static Game *s_games;
static int s_n, s_cap;
static char s_dir[16];

static void file_path(char *out, int size)
{
    snprintf(out, size, "%s/playtime.txt", s_dir);
}

static Game *find(const char *key)
{
    for (int i = 0; i < s_n; i++)
        if (!strcmp(s_games[i].key, key))
            return &s_games[i];
    return NULL;
}

/* adds `secs` to the game (a new line if it has none yet; nothing if the table is full) */
static void add(const char *key, unsigned secs)
{
    Game *g = find(key);
    if (!g) {
        if (s_n >= MAX_GAMES)
            return;
        if (s_n == s_cap) {
            int cap = s_cap ? s_cap * 2 : 64;
            Game *n = realloc(s_games, (size_t)cap * sizeof(Game));
            if (!n)
                return;
            s_games = n;
            s_cap = cap;
        }
        g = &s_games[s_n++];
        snprintf(g->key, sizeof(g->key), "%s", key);
        g->secs = 0;
    }
    g->secs = secs > MAX_SECS - g->secs ? MAX_SECS : g->secs + secs;
}

static void save(void)
{
    char path[48];
    file_path(path, sizeof(path));
    char *text = malloc((size_t)s_n * (KEY_LEN + 16) + 1);
    if (!text)
        return;
    int n = 0;
    for (int i = 0; i < s_n; i++)
        n += snprintf(text + n, KEY_LEN + 16, "%u %s\n", s_games[i].secs, s_games[i].key);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, text, n);
        sceIoClose(fd);
    }
    free(text);
}

void playtime_load(const char *dir)
{
    s_n = 0;
    snprintf(s_dir, sizeof(s_dir), "%s", dir);
    char path[48];
    file_path(path, sizeof(path));
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0 || st.st_size <= 0 || st.st_size > MAX_FILE)
        return;
    char *text = malloc((size_t)st.st_size + 1);
    SceUID fd = text ? sceIoOpen(path, PSP_O_RDONLY, 0) : -1;
    if (fd < 0) {
        free(text);
        return;
    }
    int got = sceIoRead(fd, text, (unsigned)st.st_size);
    sceIoClose(fd);
    text[got > 0 ? got : 0] = 0;
    for (char *line = text; *line;) {
        char *end = strchr(line, '\n');
        if (!end)
            break; /* a last line without its end was cut off: ignore it */
        *end = 0;
        if (end > line && end[-1] == '\r')
            end[-1] = 0;
        char *space = strchr(line, ' ');
        if (space && space > line && space[1] && strspn(line, "0123456789") == (size_t)(space - line) && space - line <= 10) {
            unsigned long long secs = strtoull(line, NULL, 10);
            *space = 0;
            if (secs > 0 && !find(space + 1))
                add(space + 1, secs > MAX_SECS ? MAX_SECS : (unsigned)secs);
        }
        line = end + 1;
    }
    free(text);
}

unsigned playtime_seconds(const Entry *e)
{
    char key[KEY_LEN];
    if (!s_n)
        return 0;
    entry_cover_key(e, key, sizeof(key));
    const Game *g = find(key);
    return g ? g->secs : 0;
}

/* --- the marker --- */

#define MARK_HEAD "psp5-ingame 1\n"

/* a 64 bit number as digits (the C library of the PSP toolchain may not do %llu) */
static char *u64_text(unsigned long long v, char *out)
{
    char tmp[24];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while (v);
    for (int i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    out[n] = 0;
    return out;
}

int playtime_mark_write(const char *path, const Entry *e, unsigned long long sys_us, unsigned long long rtc_us)
{
    char key[KEY_LEN], text[KEY_LEN + 96], sys[24], rtc[24];
    entry_cover_key(e, key, sizeof(key));
    int n = snprintf(text, sizeof(text), MARK_HEAD "sys %s\nrtc %s\nkey %s\n", u64_text(sys_us, sys), u64_text(rtc_us, rtc), key);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return 0;
    int written = sceIoWrite(fd, text, n);
    sceIoClose(fd);
    return written == n;
}

/* a line "name 123" of the marker: the number, or -1 */
static long long marker_number(const char *text, const char *name)
{
    size_t l = strlen(name);
    for (const char *p = text; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL)
        if (!strncmp(p, name, l) && p[l] == ' ' && p[l + 1] >= '0' && p[l + 1] <= '9') {
            unsigned long long v = 0;
            for (const char *d = p + l + 1; *d >= '0' && *d <= '9'; d++) {
                if (v > (~0ULL >> 1) / 10 - 1)
                    return -1; /* (too big to be a clock) */
                v = v * 10 + (unsigned)(*d - '0');
            }
            return (long long)v;
        }
    return -1;
}

static char s_short[64];

const char *playtime_last_short(void)
{
    return s_short;
}

/* printf into the report (if there is one) */
static void note(char *report, int size, const char *fmt, ...)
{
    if (!report || size <= 0)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(report, size, fmt, ap);
    va_end(ap);
}

int playtime_mark_take_ex(const char *path, unsigned long long sys_us, unsigned long long rtc_us, char *report, int report_size)
{
    note(report, report_size, "no marker: psp5 was not started again by a game");
    snprintf(s_short, sizeof(s_short), "No marker: not started by a game");
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0)
        return 0;
    char text[KEY_LEN + 160];
    int got = 0;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        got = sceIoRead(fd, text, sizeof(text) - 1);
        sceIoClose(fd);
    }
    text[got > 0 ? got : 0] = 0;
    sceIoRemove(path);
    if (strncmp(text, MARK_HEAD, strlen(MARK_HEAD))) {
        note(report, report_size, "a game ended, but the marker is empty or damaged (%d bytes): nothing counted", got > 0 ? got : 0);
        snprintf(s_short, sizeof(s_short), "Not counted: the marker is empty (%d bytes)", got > 0 ? got : 0);
        return 1; /* an empty marker (from an older psp5), or a damaged one: a game ended, but nothing is known about how long */
    }
    long long sys0 = marker_number(text, "sys"), rtc0 = marker_number(text, "rtc");
    const char *key = strstr(text, "\nkey ");
    if (!key || !key[5] || key[5] == '\n') {
        note(report, report_size, "a game ended, but the marker has no game name: nothing counted");
        snprintf(s_short, sizeof(s_short), "Not counted: the marker has no name");
        return 1;
    }
    key += 5;
    int name_len = (int)strcspn(key, "\r\n");
    if (key[name_len] != '\n') {
        note(report, report_size, "a game ended, but the marker was cut off: nothing counted");
        snprintf(s_short, sizeof(s_short), "Not counted: the marker was cut off");
        return 1; /* the end of the name is missing: the file was cut off while it was written */
    }
    char name[KEY_LEN];
    snprintf(name, sizeof(name), "%.*s", name_len, key);
    /* the two clocks; one that has gone backwards (the PSP was restarted, the clock was set) cannot be used. The system clock is also not
     * used when it says less than the shortest session while the real-time clock says more: starting a game restarts the PSP's software, which may
     * start that clock again from zero, and the new start is then a few seconds after it, not a measure of the game. */
    long long sys_d = -1, rtc_d = -1;
    if (sys0 > 0 && sys_us >= (unsigned long long)sys0) /* (0 = the clock could not be read) */
        sys_d = (long long)((sys_us - (unsigned long long)sys0) / 1000000ULL);
    if (rtc0 > 0 && rtc_us >= (unsigned long long)rtc0)
        rtc_d = (long long)((rtc_us - (unsigned long long)rtc0) / 1000000ULL);
    long long best = -1;
    const char *used = "no clock can be used";
    if (sys_d >= 0 && rtc_d >= 0) {
        if (sys_d < PLAYTIME_MIN_SECS && rtc_d >= PLAYTIME_MIN_SECS) {
            best = rtc_d;
            used = "the real-time clock (the system clock was started again by the game)";
        } else if (rtc_d < sys_d) {
            best = rtc_d;
            used = "the real-time clock (the smaller of the two)";
        } else {
            best = sys_d;
            used = "the system clock (the smaller of the two)";
        }
    } else if (rtc_d >= 0) {
        best = rtc_d;
        used = "the real-time clock (the system clock went back: the PSP was restarted)";
    } else if (sys_d >= 0) {
        best = sys_d;
        used = "the system clock (the real-time clock went back)";
    }
    char clocks[100]; /* what each clock said (to see in playtime.log which one can be trusted on a real PSP) */
    char a[16] = "unusable", b[16] = "unusable";
    if (sys_d >= 0)
        snprintf(a, sizeof(a), "%d s", (int)(sys_d > 99999999 ? 99999999 : sys_d));
    if (rtc_d >= 0)
        snprintf(b, sizeof(b), "%d s", (int)(rtc_d > 99999999 ? 99999999 : rtc_d));
    snprintf(clocks, sizeof(clocks), "system clock %s, real-time clock %s", a, b);
    if (best < PLAYTIME_MIN_SECS || best > PLAYTIME_MAX_SECS || !name[0]) {
        if (best < 0) {
            note(report, report_size, "%s: no clock can be used (%s): nothing counted", name, clocks);
            snprintf(s_short, sizeof(s_short), "Not counted: no clock can be used");
        } else {
            note(report, report_size, "%s played %d s by %s: %s, nothing counted (%s)", name, (int)best, used, best < PLAYTIME_MIN_SECS ? "too short" : "more than a day", clocks);
            if (best < PLAYTIME_MIN_SECS)
                snprintf(s_short, sizeof(s_short), "Not counted: only %d s", (int)best);
            else
                snprintf(s_short, sizeof(s_short), "Not counted: more than a day");
        }
        return 1;
    }
    add(name, (unsigned)best);
    save();
    const Game *g = find(name);
    note(report, report_size, "%s played %d s by %s: counted, %u s in all (%s)", name, (int)best, used, g ? g->secs : 0, clocks);
    char now[16], all[16];
    playtime_text((unsigned)best, now, sizeof(now));
    playtime_text(g ? g->secs : 0, all, sizeof(all));
    snprintf(s_short, sizeof(s_short), "Counted +%s, %s in all", now, all);
    return 1;
}

int playtime_mark_take(const char *path, unsigned long long sys_us, unsigned long long rtc_us)
{
    return playtime_mark_take_ex(path, sys_us, rtc_us, NULL, 0);
}

#define LOG_MAX 8000 /* bytes of playtime.log: the oldest lines go first */

void playtime_log(const char *when, const char *msg)
{
    if (!s_dir[0])
        return;
    char path[48];
    snprintf(path, sizeof(path), "%s/playtime.log", s_dir);
    char line[300];
    int len = snprintf(line, sizeof(line), "%s  %s\n", when ? when : "", msg ? msg : "");
    if (len >= (int)sizeof(line)) {
        len = (int)sizeof(line) - 1; /* a long line is cut, but still ends with a newline */
        line[len - 1] = '\n';
    }
    for (int i = 0; i < len - 1; i++)
        if (line[i] == '\n' || line[i] == '\r')
            line[i] = ' ';
    char *old = malloc(LOG_MAX + 1);
    if (!old)
        return;
    int n = 0;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        n = sceIoRead(fd, old, LOG_MAX);
        sceIoClose(fd);
        if (n < 0)
            n = 0;
    }
    int skip = 0;
    if (n + len > LOG_MAX) { /* drop whole lines from the start until the new one fits */
        skip = n + len - LOG_MAX;
        while (skip < n && old[skip - 1] != '\n')
            skip++;
        if (skip > n)
            skip = n;
    }
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        if (n > skip)
            sceIoWrite(fd, old + skip, n - skip);
        sceIoWrite(fd, line, len);
        sceIoClose(fd);
    }
    free(old);
}

void playtime_text(unsigned secs, char *out, int size)
{
    out[0] = 0;
    if (secs == 0)
        return;
    if (secs < 60) { /* something was played, but not a minute yet */
        snprintf(out, size, "<1%s", L(S_UNIT_MIN));
        return;
    }
    unsigned mins = secs / 60;
    if (mins < 60)
        snprintf(out, size, "%u%s", mins, L(S_UNIT_MIN));
    else
        snprintf(out, size, "%u%s", mins / 60 > 9999 ? 9999 : mins / 60, L(S_UNIT_H));
}
