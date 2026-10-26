/* psp5-authentic - see awake.h */
#include "awake.h"

#define HEAD  "psp5-awake 1\n"
#define RTC   "rtc "
#define AWAKE "awake "
#define END   "end\n"

static int starts(const char *s, int left, const char *w)
{
    int n = 0;
    while (w[n]) {
        if (n >= left || s[n] != w[n])
            return 0;
        n++;
    }
    return n;
}

#define MAX_SECS 0xFFFFFF00u

void awake_start(AwakeClock *c, unsigned long long now_us, unsigned secs0)
{
    c->last_us = now_us;
    c->secs = secs0 > MAX_SECS ? MAX_SECS : secs0;
    c->rem_us = 0;
}

void awake_add_secs(AwakeClock *c, unsigned secs)
{
    c->secs = secs > MAX_SECS - c->secs ? MAX_SECS : c->secs + secs;
}

unsigned awake_tick(AwakeClock *c, unsigned long long now_us)
{
    if (now_us > c->last_us && now_us - c->last_us <= AWAKE_MAX_STEP_US) {
        c->rem_us += (unsigned)(now_us - c->last_us);
        while (c->rem_us >= 1000000u) {
            c->rem_us -= 1000000u;
            if (c->secs < MAX_SECS)
                c->secs++;
        }
    }
    c->last_us = now_us; /* a long step (sleep) or a step back is not counted, and counting goes on from here */
    return c->secs;
}

/* digits[0..n) -> token without leading zeros; 0 when there are none or too many */
static int copy_number(const char *d, int n, char *token, int size)
{
    int i = 0;
    while (i < n - 1 && d[i] == '0')
        i++;
    n -= i;
    if (n < 1 || n > 20 || n >= size)
        return 0;
    for (int k = 0; k < n; k++) {
        if (d[i + k] < '0' || d[i + k] > '9')
            return 0;
        token[k] = d[i + k];
    }
    token[n] = 0;
    return 1;
}

int awake_token(const char *marker, int len, char *token, int size)
{
    if (!marker || !token || size < 2)
        return 0;
    for (int i = 0; i < len; i++) {
        if ((i == 0 || marker[i - 1] == '\n') && starts(marker + i, len - i, RTC)) {
            int s = i + 4, e = s;
            while (e < len && marker[e] >= '0' && marker[e] <= '9')
                e++;
            if (e >= len || (marker[e] != '\n' && marker[e] != '\r')) /* the line must end after the digits */
                return 0;
            return copy_number(marker + s, e - s, token, size);
        }
    }
    return 0;
}

int awake_format(char *out, int size, const char *token, unsigned secs)
{
    int tl = 0;
    while (token && token[tl])
        tl++;
    if (tl < 1 || tl > 20)
        return 0;
    int need = (int)sizeof(HEAD) - 1 + 4 + tl + 1 + 6 + 10 + 1 + (int)sizeof(END) - 1;
    if (!out || size < need + 1)
        return 0;
    int n = 0;
    for (const char *p = HEAD; *p; p++) out[n++] = *p;
    for (const char *p = RTC; *p; p++) out[n++] = *p;
    for (int i = 0; i < tl; i++) out[n++] = token[i];
    out[n++] = '\n';
    for (const char *p = AWAKE; *p; p++) out[n++] = *p;
    for (int i = 9; i >= 0; i--) {
        out[n + i] = (char)('0' + secs % 10);
        secs /= 10;
    }
    n += 10;
    out[n++] = '\n';
    for (const char *p = END; *p; p++) out[n++] = *p;
    out[n] = 0;
    return n;
}

int awake_parse(const char *buf, int len, char *token, int size, unsigned *secs)
{
    int n = starts(buf, len, HEAD);
    if (!n)
        return 0;
    int p = n;
    if (!starts(buf + p, len - p, RTC))
        return 0;
    p += 4;
    int e = p;
    while (e < len && buf[e] >= '0' && buf[e] <= '9')
        e++;
    if (e >= len || buf[e] != '\n' || !copy_number(buf + p, e - p, token, size))
        return 0;
    p = e + 1;
    if (!starts(buf + p, len - p, AWAKE))
        return 0;
    p += 6;
    if (len - p < 10 + 1)
        return 0;
    unsigned long long v = 0;
    for (int i = 0; i < 10; i++) {
        if (buf[p + i] < '0' || buf[p + i] > '9')
            return 0;
        v = v * 10 + (unsigned)(buf[p + i] - '0');
    }
    if (v > 0xFFFFFFFFULL || buf[p + 10] != '\n')
        return 0;
    p += 11;
    if (!starts(buf + p, len - p, END))
        return 0;
    *secs = (unsigned)v;
    return 1;
}

/* the last part of the path, and the one before it, in capitals-insensitive form */
static int same_name(const char *a, int alen, const char *b)
{
    int i = 0;
    for (; b[i]; i++) {
        char x = i < alen ? a[i] : 0;
        if (x >= 'a' && x <= 'z')
            x = (char)(x - 'a' + 'A');
        if (x != b[i])
            return 0;
    }
    return i == alen;
}

int awake_is_psp5(const char *init_file)
{
    if (!init_file)
        return 0;
    int len = 0;
    while (init_file[len])
        len++;
    int end = len, start = len;
    while (start > 0 && init_file[start - 1] != '/' && init_file[start - 1] != ':')
        start--;
    if (same_name(init_file + start, end - start, "VBOOT.PBP"))
        return 1;
    if (!same_name(init_file + start, end - start, "EBOOT.PBP") || start < 2 || init_file[start - 1] != '/')
        return 0;
    int dend = start - 1, dstart = dend;
    while (dstart > 0 && init_file[dstart - 1] != '/' && init_file[dstart - 1] != ':')
        dstart--;
    return same_name(init_file + dstart, dend - dstart, "PSP5");
}
