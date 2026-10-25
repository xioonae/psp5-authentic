/* psp5 – made by Mille Weber */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "folders.h"

/* Copies at most n bytes of s to out (room `size`), never in the middle of a UTF-8 letter, always terminated. */
static void copy_utf8(char *out, int size, const char *s, int n)
{
    if (size <= 0)
        return;
    if (n > size - 1)
        n = size - 1;
    while (n > 0 && n < (int)strlen(s) && ((unsigned char)s[n] & 0xC0) == 0x80)
        n--; /* the cut would fall inside a letter: leave the letter out */
    memcpy(out, s, n);
    out[n] = 0;
}

/* --- where a game is --- */

int folder_raw(const char *path, char *out, int size)
{
    if (size > 0)
        out[0] = 0;
    if (!path || size <= 0)
        return 0;
    int iso = 0;
    const char *rest = NULL;
    for (const char *p = path; *p && !rest; p++) { /* the first of the two roots: ms0:/ISO/ or ms0:/PSP/GAME/ (also ef0:) */
        if (*p != '/')
            continue;
        if (!strncasecmp(p, "/PSP/GAME/", 10))
            rest = p + 10;
        else if (!strncasecmp(p, "/ISO/", 5)) {
            rest = p + 5;
            iso = 1;
        }
    }
    if (!rest)
        return 0;
    /* the parts after the root: [folders...] [game folder] EBOOT.PBP, or [folders...] image */
    int parts = 1;
    for (const char *p = rest; *p; p++)
        if (*p == '/' && p[1])
            parts++;
    int skip = iso ? 2 : 3; /* the last folder before the file (ISO), or before the game's own folder (EBOOT) */
    if (parts < skip)
        return 0;
    const char *start = rest;
    for (int k = parts - skip; k > 0; k--) /* the part that is the folder: parts - skip folders come before it */
        start = strchr(start, '/') + 1;
    const char *end = strchr(start, '/');
    int n = end ? (int)(end - start) : (int)strlen(start);
    if (n <= 0)
        return 0;
    copy_utf8(out, size, start, n);
    return out[0] != 0;
}

/* --- the name --- */

static int is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

void folder_clean(const char *raw, char *out, int size)
{
    const char *p = raw;
    if (!strncasecmp(p, "CAT_", 4))
        p += 4;
    if (is_digit(p[0]) && is_digit(p[1]) && p[2] && !is_digit(p[2]))
        p += 2;
    while (*p == ' ')
        p++;
    if (!*p)
        p = raw; /* nothing is left: the name was only the prefix */
    copy_utf8(out, size, p, (int)strlen(p));
}

/* --- the letter --- */

/* the capital of a letter (a code point), for the alphabets a folder name is likely to start with */
static unsigned upper_of(unsigned c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
        return c - 0x20;
    if (c == 0xFF)
        return 0x178;
    if ((c >= 0x100 && c <= 0x137 && (c & 1)) || (c >= 0x14A && c <= 0x177 && (c & 1)))
        return c == 0x131 ? 'I' : c - 1;
    if ((c >= 0x139 && c <= 0x148 && !(c & 1)) || (c >= 0x179 && c <= 0x17E && !(c & 1)))
        return c - 1;
    if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2)
        return c - 0x20;
    if (c >= 0x430 && c <= 0x44F)
        return c - 0x20;
    if (c >= 0x450 && c <= 0x45F)
        return c - 0x50;
    return c;
}

static int put_utf8(char *out, unsigned c)
{
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (c >> 12));
    out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    out[2] = (char)(0x80 | (c & 0x3F));
    return 3;
}

void folder_initial(const char *name, char *out, int size)
{
    static const char *const pairs[] = { "Ch", "Dz", "IJ", "Lj", "Ll", "Nj" }; /* how each is written as a capital */
    if (size <= 0)
        return;
    out[0] = 0;
    const unsigned char *s = (const unsigned char *)name;
    while (*s == ' ' || *s == '_' || *s == '-' || *s == '.' || *s == '[' || *s == '(')
        s++; /* (a name like "[Emu]" or "_old" is about what comes after the sign) */
    if (!*s)
        return;
    for (unsigned k = 0; k < sizeof pairs / sizeof pairs[0]; k++)
        if (toupper(s[0]) == pairs[k][0] && tolower(s[1]) == tolower((unsigned char)pairs[k][1]) && size >= 3) {
            out[0] = pairs[k][0];
            out[1] = pairs[k][1];
            out[2] = 0;
            return;
        }
    unsigned c = s[0];
    int len = 1;
    if (c >= 0xF0) {
        len = 4;
    } else if (c >= 0xE0) {
        c &= 0x0F;
        len = 3;
    } else if (c >= 0xC0) {
        c &= 0x1F;
        len = 2;
    }
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) { /* damaged UTF-8: the byte is copied as it is */
            if (size > 1) {
                out[0] = (char)s[0];
                out[1] = 0;
            }
            return;
        }
        c = (c << 6) | (s[i] & 0x3F);
    }
    if (len == 4) { /* (outside the letters of any font here: copied) */
        if (size > 4) {
            memcpy(out, s, 4);
            out[4] = 0;
        }
        return;
    }
    char buf[4];
    int n = put_utf8(buf, upper_of(c));
    if (n < size) {
        memcpy(out, buf, n);
        out[n] = 0;
    }
}

void folder_upper(const char *s, char *out, int size)
{
    int n = 0;
    if (size <= 0)
        return;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        unsigned c = *p;
        int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        int ok = 1;
        for (int i = 1; i < len; i++)
            if ((p[i] & 0xC0) != 0x80) { /* damaged UTF-8: the byte is copied as it is */
                ok = 0;
                break;
            }
        char buf[4];
        int m;
        if (!ok || len == 4) { /* (a letter outside every alphabet here, or damage: unchanged) */
            m = ok ? 4 : 1;
            memcpy(buf, p, m);
        } else {
            c = len == 3 ? c & 0x0F : len == 2 ? c & 0x1F : c;
            for (int i = 1; i < len; i++)
                c = (c << 6) | (p[i] & 0x3F);
            m = put_utf8(buf, upper_of(c));
        }
        if (n + m >= size)
            break;
        memcpy(out + n, buf, m);
        n += m;
        p += ok ? len : 1;
    }
    out[n] = 0;
}

/* --- all the folders --- */

typedef struct {
    char name[FOLDER_NAME_MAX];
    int entry;
} Member;

static int cmp_member(const void *a, const void *b)
{
    const Member *x = a, *y = b;
    int c = strcasecmp(x->name, y->name);
    return c ? c : x->entry - y->entry;
}

void folders_build(const EntryList *list, FolderSet *fs)
{
    memset(fs, 0, sizeof(*fs));
    fs->of = malloc((list->count ? list->count : 1) * sizeof(int));
    Member *m = malloc((list->count ? list->count : 1) * sizeof(Member));
    if (!fs->of || !m) {
        free(fs->of);
        free(m);
        memset(fs, 0, sizeof(*fs));
        return;
    }
    fs->of_count = list->count;
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        char raw[FOLDER_NAME_MAX];
        fs->of[i] = -1;
        if (folder_raw(list->items[i].path, raw, sizeof(raw))) {
            folder_clean(raw, m[n].name, sizeof(m[n].name));
            m[n++].entry = i;
        }
    }
    qsort(m, n, sizeof(Member), cmp_member);
    fs->items = malloc((n ? n : 1) * sizeof(Folder));
    if (!fs->items) {
        free(m);
        return;
    }
    for (int k = 0; k < n; k++) {
        if (k == 0 || strcasecmp(m[k].name, m[k - 1].name)) {
            Folder *f = &fs->items[fs->count++];
            memset(f, 0, sizeof(*f));
            memcpy(f->name, m[k].name, sizeof(f->name));
        }
        Folder *f = &fs->items[fs->count - 1];
        fs->of[m[k].entry] = fs->count - 1;
        f->count++;
        if (list->items[m[k].entry].file_time > f->newest)
            f->newest = list->items[m[k].entry].file_time;
    }
    free(m);
}

void folders_free(FolderSet *fs)
{
    free(fs->items);
    free(fs->of);
    memset(fs, 0, sizeof(*fs));
}

int folders_find(const FolderSet *fs, const char *name)
{
    for (int i = 0; i < fs->count; i++)
        if (!strcasecmp(fs->items[i].name, name))
            return i;
    return -1;
}
