/* psp5 – made by Mille Weber */
/* How long each game has been played (see playtime.h). */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "playtime.h"
#include "awake.h"
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

/* The seconds the helper plugin counted (src/awake.h), when its file is whole and belongs to this marker; else -1. The file is removed
 * either way: it is only for one game. */
static long long awake_read(const char *path, const char *marker_text, int marker_len)
{
    char apath[200], buf[AWAKE_FILE_MAX + 32], file_token[AWAKE_TOKEN_MAX], mark_token[AWAKE_TOKEN_MAX];
    unsigned secs = 0;
    long long ret = -1;
    snprintf(apath, sizeof(apath), "%s.awake", path);
    SceUID fd = sceIoOpen(apath, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int n = sceIoRead(fd, buf, sizeof(buf));
    sceIoClose(fd);
    sceIoRemove(apath);
    if (n > 0 && marker_text && awake_parse(buf, n, file_token, sizeof(file_token), &secs) &&
        awake_token(marker_text, marker_len, mark_token, sizeof(mark_token)) && strcmp(mark_token, "0") && !strcmp(file_token, mark_token))
        ret = secs;
    return ret;
}

int playtime_mark_take(const char *path, unsigned long long sys_us, unsigned long long rtc_us)
{
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0) {
        char apath[200]; /* a file of the plugin without a marker is left over from a game that was never counted */
        snprintf(apath, sizeof(apath), "%s.awake", path);
        sceIoRemove(apath);
        return 0;
    }
    char text[KEY_LEN + 160];
    int got = 0;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        got = sceIoRead(fd, text, sizeof(text) - 1);
        sceIoClose(fd);
    }
    text[got > 0 ? got : 0] = 0;
    long long awake = awake_read(path, text, got > 0 ? got : 0);
    sceIoRemove(path);
    if (strncmp(text, MARK_HEAD, strlen(MARK_HEAD)))
        return 1; /* an empty marker (from an older psp5), or a damaged one: a game ended, but nothing is known about how long */
    long long sys0 = marker_number(text, "sys"), rtc0 = marker_number(text, "rtc");
    const char *key = strstr(text, "\nkey ");
    if (!key || !key[5] || key[5] == '\n')
        return 1;
    key += 5;
    int name_len = (int)strcspn(key, "\r\n");
    if (key[name_len] != '\n')
        return 1; /* the end of the name is missing: the file was cut off while it was written */
    char name[KEY_LEN];
    snprintf(name, sizeof(name), "%.*s", name_len, key);
    /* The two clocks; one that has gone backwards (the PSP was restarted, the clock was set) or could not be read (0) cannot be used.
     * Starting a game and leaving it restart the PSP's software and the system clock with it (seen in a log from a real PSP: 7 s when the game
     * was started, 3 s when psp5 was started again after it), so mostly only the real-time clock is left. The system clock is also not used
     * when it says less than the shortest session while the real-time clock says more: it has started again from zero and come back to
     * a few seconds above the old reading, which is not a measure of the game. */
    long long sys_d = -1, rtc_d = -1;
    if (sys0 > 0 && sys_us >= (unsigned long long)sys0)
        sys_d = (long long)((sys_us - (unsigned long long)sys0) / 1000000ULL);
    if (rtc0 > 0 && rtc_us >= (unsigned long long)rtc0)
        rtc_d = (long long)((rtc_us - (unsigned long long)rtc0) / 1000000ULL);
    long long best = -1;
    if (awake >= 0) /* the plugin counted only the time the PSP was awake: that is the play time (never more than the time that went by) */
        best = rtc_d >= 0 && awake > rtc_d ? rtc_d : awake;
    else if (sys_d >= 0 && rtc_d >= 0)
        best = (sys_d < PLAYTIME_MIN_SECS && rtc_d >= PLAYTIME_MIN_SECS) || rtc_d < sys_d ? rtc_d : sys_d;
    else if (rtc_d >= 0)
        best = rtc_d;
    else if (sys_d >= 0)
        best = sys_d;
    if (best >= PLAYTIME_MIN_SECS && best <= PLAYTIME_MAX_SECS && name[0]) {
        add(name, (unsigned)best);
        save();
    }
    return 1;
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
