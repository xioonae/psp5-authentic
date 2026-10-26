/* psp5-authentic - see awake.h */
#include "awake.h"

#define HEAD  "psp5-played 1\n"
#define SECS  "secs "
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

int awake_format(char *out, int size, unsigned secs)
{
    int need = (int)sizeof(HEAD) - 1 + (int)sizeof(SECS) - 1 + 10 + 1 + (int)sizeof(END) - 1;
    if (!out || size < need + 1)
        return 0;
    int n = 0;
    for (const char *p = HEAD; *p; p++) out[n++] = *p;
    for (const char *p = SECS; *p; p++) out[n++] = *p;
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

int awake_parse(const char *buf, int len, unsigned *secs)
{
    int p = starts(buf, len, HEAD);
    if (!p || !starts(buf + p, len - p, SECS))
        return 0;
    p += (int)sizeof(SECS) - 1;
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

/* --- the name of a game --- */

static int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

int awake_key_from_path(const char *init_file, char *key, int size)
{
    if (!init_file || !key || size < 2)
        return 0;
    int len = str_len(init_file), bstart = len;
    while (bstart > 0 && init_file[bstart - 1] != '/' && init_file[bstart - 1] != ':')
        bstart--;
    if (bstart >= len)
        return 0; /* no file name at all */
    if (same_name(init_file + bstart, len - bstart, "EBOOT.PBP")) {
        /* as entry_cover_key() does it for a game that is a folder: the name between the last two slashes */
        int end = len;
        while (end > 0 && init_file[end - 1] != '/')
            end--;
        if (end <= 0)
            return 0;
        end--; /* the slash */
        int start = end;
        while (start > 0 && init_file[start - 1] != '/')
            start--;
        if (end <= start)
            return 0;
        int n = end - start;
        if (n > size - 1)
            n = size - 1; /* (snprintf cuts it there too) */
        for (int i = 0; i < n; i++) {
            if (init_file[start + i] == ':')
                return 0; /* "ms0:" is a drive, not the folder of a game */
            key[i] = init_file[start + i];
        }
        key[n] = 0;
        return 1;
    }
    /* another .PBP (ARK's menu, a launcher) is not a game; PBOOT.PBP is a PSP game installed with its update */
    if (len >= 4 && same_name(init_file + len - 4, 4, ".PBP") && !same_name(init_file + bstart, len - bstart, "PBOOT.PBP"))
        return 0;
    return 2;
}

static unsigned rd16(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static unsigned rd32(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

int awake_key_from_sfo(const unsigned char *sfo, unsigned len, char *key, int size)
{
    static const char NAME[] = "DISC_ID";
    if (!sfo || !key || size < 2 || len < 20 || rd32(sfo) != 0x46535000u)
        return 0;
    unsigned key_table = rd32(sfo + 8), data_table = rd32(sfo + 12), count = rd32(sfo + 16);
    if (count > (len - 20) / 16)
        return 0;
    for (unsigned i = 0; i < count; i++) {
        const unsigned char *e = sfo + 20 + i * 16;
        unsigned long long koff = (unsigned long long)key_table + rd16(e);
        if (koff >= len)
            continue;
        int k = 0;
        while (NAME[k] && koff + (unsigned)k < len && sfo[koff + (unsigned)k] == (unsigned char)NAME[k])
            k++;
        if (NAME[k] || koff + (unsigned)k >= len || sfo[koff + (unsigned)k] != 0)
            continue;
        unsigned long long doff = (unsigned long long)data_table + rd32(e + 12);
        unsigned dlen = rd32(e + 4);
        if (doff + dlen > len)
            return 0;
        unsigned n = dlen;
        if (n > AWAKE_GAME_ID_MAX - 1)
            n = AWAKE_GAME_ID_MAX - 1;
        if (n > (unsigned)size - 1)
            n = (unsigned)size - 1;
        unsigned m = 0;
        while (m < n && sfo[doff + m]) { /* a C string, as psp5 reads it */
            unsigned char c = sfo[doff + m];
            if (c < 0x20 || c >= 0x7F || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                return 0; /* not a name that can be a file */
            key[m] = (char)c;
            m++;
        }
        key[m] = 0;
        return m > 0;
    }
    return 0;
}

/* --- the files of the plugin --- */

int awake_dir_of(const char *module_path, char *out, int size)
{
    if (!module_path || !out)
        return 0;
    int len = str_len(module_path), slash = len;
    while (slash > 0 && module_path[slash - 1] != '/')
        slash--;
    slash--; /* the slash itself */
    if (slash <= 0 || slash >= size)
        return 0;
    for (int i = 0; i < slash; i++)
        out[i] = module_path[i];
    out[slash] = 0;
    return 1;
}

int awake_played_dir(const char *dir, char *out, int size)
{
    const char *tail = "/" AWAKE_PLAYED_DIR;
    int d = dir ? str_len(dir) : 0, t = str_len(tail);
    if (!out || d < 1 || d + t >= size)
        return 0;
    for (int i = 0; i < d; i++) out[i] = dir[i];
    for (int i = 0; i <= t; i++) out[d + i] = tail[i];
    return 1;
}

int awake_played_path(const char *dir, const char *key, char *out, int size)
{
    int k = key ? str_len(key) : 0, e = str_len(AWAKE_PLAYED_EXT);
    if (k < 1 || k >= AWAKE_KEY_MAX)
        return 0;
    for (int i = 0; i < k; i++)
        if (key[i] == '/')
            return 0;
    if (!awake_played_dir(dir, out, size))
        return 0;
    int d = str_len(out);
    if (d + 1 + k + e >= size)
        return 0;
    out[d++] = '/';
    for (int i = 0; i < k; i++) out[d++] = key[i];
    for (int i = 0; i <= e; i++) out[d + i] = AWAKE_PLAYED_EXT[i];
    return 1;
}

int awake_file_key(const char *file_name, char *key, int size)
{
    if (!file_name || !key)
        return 0;
    int len = str_len(file_name), e = str_len(AWAKE_PLAYED_EXT), n = len - e;
    if (n < 1 || n >= size || n >= AWAKE_KEY_MAX || !same_name(file_name + n, e, ".TXT"))
        return 0;
    for (int i = 0; i < n; i++)
        key[i] = file_name[i];
    key[n] = 0;
    return 1;
}
