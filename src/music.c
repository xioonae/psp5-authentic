/* psp5 – made by Mille Weber */
/* The music player. The Music screen lists a folder of ms0:/MUSIC (or ef0:/MUSIC) at a time and plays the songs of the folder that is open; the sound is made by the
 * PSP's own decoders (sceMp3 here; more formats behind the same small interface, see Codec) in a thread of its own, through the SRC audio channel, which does 44.1 kHz,
 * 48 kHz and the others. The screen steers the thread with a small queue of commands, so that nothing has to be locked. The buffers are small (~50 KB), which is
 * enough for a PSP-1000. Only the song that is playing has its tags read (when it starts): there is no scan of the whole card. */
#include <pspkernel.h>
#include <pspaudio.h>
#include <psputility.h>
#include <pspmp3.h>
#include <pspatrac3.h>
#include <pspsysmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "music.h"
#include "system.h"

#define MAX_ENTRIES  1000   /* lines of one folder in the Music screen, and songs in a queue */
#define QNAME        128    /* a name in the queue (the same size as MusicEntry.name) */
#define TAG_READ     8192   /* how much of an ID3v2 tag is read for the title, the artist and the album */
#define MP3_BUF      (32 * 1024)
#define PCM_BUF      (1152 * 2 * 2 * 2) /* the decoder double buffers in this one itself */
#define MAX_SAMPLES  4096   /* the most that one frame of any codec has per channel (the SRC channel takes 17 to 4111) */
#define MP3_END      0x80671402 /* sceMp3Decode: the stream is over */
#define ALREADY_LOADED  0x80020139
#define ALREADY_LOADED2 0x80111102 /* the same, from sceUtilityLoadModule */

/* the tags of a song while they are read (the track number is of no use here, but the reader of ID3 keeps it) */
typedef struct {
    Track t;
    int trackno;
} Item;

/* --- what the screen reads (the thread writes it) --- */

static volatile int s_state = MUSIC_STOPPED;
static volatile float s_elapsed, s_duration;
static volatile int s_gen;          /* another song has started */
static volatile int s_now_seq;      /* the tags below are written between an odd and an even number, so that a reader can tell a half written copy */
static Track s_now;
static volatile int s_now_valid;
static volatile int s_cjk;
static volatile int s_shuffle, s_repeat;
static char s_dir_pub[256];         /* the folder whose songs are playing */

/* --- commands from the screen (one writer, one reader) --- */

enum { CMD_LOAD, CMD_PAUSE, CMD_STOP, CMD_NEXT, CMD_PREV, CMD_SEEK, CMD_SHUFFLE };
#define QUEUE 16
static volatile int s_q_cmd[QUEUE], s_q_arg[QUEUE];
static volatile int s_q_head, s_q_tail; /* head is written by the screen only, tail by the thread only */
static int s_in_cmd;                    /* 1 while the thread does a command (it stays in the queue until it is done) */

/* 1 if the screen has sent a command that the thread has not begun (not the one it is doing now) */
static int command_waiting(void)
{
    return (s_q_head - s_q_tail + QUEUE) % QUEUE > s_in_cmd;
}

/* --- thread and decoder --- */

static SceUID s_thread = -1;
static volatile int s_quit;
static int s_ready;                      /* modules and resource loaded */
static int s_loaded_avcodec, s_loaded_mp3, s_loaded_atrac;

static unsigned char __attribute__((aligned(64))) s_mp3buf[MP3_BUF];
static unsigned char __attribute__((aligned(64))) s_pcmbuf[PCM_BUF];
static short __attribute__((aligned(64))) s_out[2][MAX_SAMPLES * 2];
static int s_out_idx;

/* a codec: one kind of song file. The player has the file open (s_fd, from s_start to s_size) and the codec sets the rate, the channels and the length (and s_pos as it decodes) */
typedef struct {
    int (*open)(void);                                                  /* 0 = ready to decode, -1 = this file cannot be played */
    int (*frame)(const short **pcm, int *samples, int *channels);       /* 1 = a frame of `samples` per channel (0 samples = nothing this time), 0 = over, -1 = error */
    int (*seek)(float sec);                                             /* 0 = done, -1 = past the end */
    void (*close)(void);
} Codec;

static const Codec *s_codec;
static SceUID s_fd = -1;
static char s_path[256];                 /* the song that is open (the file is opened again when the PSP has slept) */
static int s_rate, s_channels;
static int s_src_rate, s_src_samples;    /* the SRC channel that is reserved, 0 = none */
static u32 s_start, s_size;              /* where the stream starts (after the ID3 tag) and ends */
static float s_decoded;                  /* where the codec has decoded to, seconds (what the song has played is s_elapsed, set when the sound is out) */

/* ---------------------------------------------------------------------- */
/* ID3 */

static u32 syncsafe(const unsigned char *p)
{
    return ((u32)(p[0] & 0x7F) << 21) | ((u32)(p[1] & 0x7F) << 14) | ((u32)(p[2] & 0x7F) << 7) | (p[3] & 0x7F);
}

static u32 be32(const unsigned char *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

/* Skriver ett tecken som UTF-8; returnerar 0 om det inte fick plats */
static int put_utf8(char *out, int *n, int size, u32 c)
{
    char tmp[4];
    int len;
    if (c < 0x80) { tmp[0] = c; len = 1; }
    else if (c < 0x800) { tmp[0] = 0xC0 | (c >> 6); tmp[1] = 0x80 | (c & 0x3F); len = 2; }
    else if (c < 0x10000) { tmp[0] = 0xE0 | (c >> 12); tmp[1] = 0x80 | ((c >> 6) & 0x3F); tmp[2] = 0x80 | (c & 0x3F); len = 3; }
    else { tmp[0] = 0xF0 | (c >> 18); tmp[1] = 0x80 | ((c >> 12) & 0x3F); tmp[2] = 0x80 | ((c >> 6) & 0x3F); tmp[3] = 0x80 | (c & 0x3F); len = 4; }
    if (*n + len >= size)
        return 0;
    memcpy(out + *n, tmp, len);
    *n += len;
    return 1;
}

static void trim(char *s)
{
    int n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
    int lead = 0;
    while (s[lead] == ' ')
        lead++;
    if (lead)
        memmove(s, s + lead, n - lead + 1);
}

/* Textram (kodningsbyte + text) → UTF-8 */
static void id3_text(const unsigned char *p, int len, char *out, int size)
{
    int n = 0;
    out[0] = 0;
    if (len < 1)
        return;
    int enc = p[0];
    p++;
    len--;
    if (enc == 1 || enc == 2) { /* UTF-16 med BOM / UTF-16BE */
        int big = 1;
        if (enc == 1 && len >= 2) {
            if (p[0] == 0xFF && p[1] == 0xFE) { big = 0; p += 2; len -= 2; }
            else if (p[0] == 0xFE && p[1] == 0xFF) { p += 2; len -= 2; }
        }
        for (int i = 0; i + 1 < len; i += 2) {
            u32 c = big ? (p[i] << 8 | p[i + 1]) : (p[i + 1] << 8 | p[i]);
            if (!c)
                break;
            if (c >= 0xD800 && c < 0xDC00 && i + 3 < len) { /* surrogatpar */
                u32 lo = big ? (p[i + 2] << 8 | p[i + 3]) : (p[i + 3] << 8 | p[i + 2]);
                if (lo >= 0xDC00 && lo < 0xE000) {
                    c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                    i += 2;
                }
            }
            if (!put_utf8(out, &n, size, c))
                break;
        }
    } else if (enc == 3) { /* UTF-8 */
        int i = 0;
        while (i < len && p[i] && n + 1 < size)
            out[n++] = p[i++];
        /* fick inte allt plats: skär inte mitt i ett tecken */
        if (i < len && p[i])
            while (n > 0 && (p[n] & 0xC0) == 0x80)
                n--;
    } else { /* Latin-1 */
        for (int i = 0; i < len && p[i]; i++)
            if (!put_utf8(out, &n, size, p[i]))
                break;
    }
    out[n] = 0;
    trim(out);
}

/* Storleken på ID3v2-taggen i början av filen (0 om ingen) */
static u32 id3v2_size(SceUID fd)
{
    unsigned char h[10];
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    if (sceIoRead(fd, h, 10) != 10 || memcmp(h, "ID3", 3) || h[3] == 0xFF)
        return 0;
    return 10 + syncsafe(h + 6) + ((h[5] & 0x10) ? 10 : 0);
}

/* Läser titel, artist, album och spårnummer ur ID3v2 (v2.2–v2.4) och ID3v1. */
static void read_tags(SceUID fd, u32 file_size, Item *it, unsigned char *buf)
{
    unsigned char h[10];
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    if (sceIoRead(fd, h, 10) == 10 && !memcmp(h, "ID3", 3) && h[3] >= 2 && h[3] <= 4) {
        int ver = h[3], flags = h[5];
        u32 size = syncsafe(h + 6);
        int len = size < TAG_READ ? (int)size : TAG_READ;
        len = sceIoRead(fd, buf, len);
        if (len > 0) {
            /* v2.2/v2.3: hela taggen kan vara "unsynchronised" (FF 00 → FF) */
            if ((flags & 0x80) && ver < 4) {
                int w = 0;
                for (int r = 0; r < len; r++) {
                    buf[w++] = buf[r];
                    if (buf[r] == 0xFF && r + 1 < len && buf[r + 1] == 0)
                        r++;
                }
                len = w;
            }
            int pos = 0;
            if ((flags & 0x40) && ver >= 3 && len >= 4) { /* utökad header */
                u32 ext = ver == 3 ? be32(buf) : syncsafe(buf);
                /* trasig storlek: hoppa över ramarna i stället för att läsa utanför */
                pos = ext < (u32)len - 4 ? (int)ext + (ver == 3 ? 4 : 0) : len;
            }
            int idlen = ver == 2 ? 3 : 4, hdr = ver == 2 ? 6 : 10;
            while (pos + hdr <= len && buf[pos]) {
                const unsigned char *f = buf + pos;
                u32 fsize = ver == 2 ? ((u32)f[3] << 16 | f[4] << 8 | f[5]) : ver == 4 ? syncsafe(f + 4) : be32(f + 4);
                if (fsize == 0 || fsize > (u32)(len - pos - hdr)) /* även "negativa" storlekar */
                    break;
                const unsigned char *d = f + hdr;
                if (!memcmp(f, ver == 2 ? "TT2" : "TIT2", idlen))
                    id3_text(d, fsize, it->t.title, sizeof(it->t.title));
                else if (!memcmp(f, ver == 2 ? "TP1" : "TPE1", idlen))
                    id3_text(d, fsize, it->t.artist, sizeof(it->t.artist));
                else if (!memcmp(f, ver == 2 ? "TAL" : "TALB", idlen))
                    id3_text(d, fsize, it->t.album, sizeof(it->t.album));
                else if (!memcmp(f, ver == 2 ? "TRK" : "TRCK", idlen)) {
                    char num[16];
                    id3_text(d, fsize, num, sizeof(num));
                    it->trackno = atoi(num);
                }
                pos += hdr + fsize;
            }
        }
    }
    /* ID3v1 sist i filen, om något fortfarande saknas */
    if ((!it->t.title[0] || !it->t.artist[0] || !it->t.album[0]) && file_size > 128 + 10) {
        unsigned char v1[128];
        sceIoLseek32(fd, file_size - 128, PSP_SEEK_SET);
        if (sceIoRead(fd, v1, 128) == 128 && !memcmp(v1, "TAG", 3)) {
            char *fields[3] = { it->t.title, it->t.artist, it->t.album };
            int sizes[3] = { sizeof(it->t.title), sizeof(it->t.artist), sizeof(it->t.album) };
            for (int k = 0; k < 3; k++) {
                if (fields[k][0])
                    continue;
                unsigned char tmp[32];
                tmp[0] = 0; /* Latin-1 */
                memcpy(tmp + 1, v1 + 3 + k * 30, 30);
                id3_text(tmp, 31, fields[k], sizes[k]);
            }
            if (!it->trackno && v1[125] == 0 && v1[126])
                it->trackno = v1[126];
        }
    }
}

/* Tar bort ID3:s "unsynchronisation" (FF 00 → FF). Returnerar ny längd. */
static int unsync(unsigned char *b, int len)
{
    int w = 0;
    for (int r = 0; r < len; r++) {
        b[w++] = b[r];
        if (b[r] == 0xFF && r + 1 < len && b[r + 1] == 0)
            r++;
    }
    return w;
}

#define MAX_COVER_TAG (2 * 1024 * 1024) /* större taggar läses inte (minnet på PSP-1000) */

void *music_cover_data(const char *path, u32 *size_out)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0)
        return NULL;
    unsigned char h[10];
    unsigned char *buf = NULL;
    int len = 0;
    if (sceIoRead(fd, h, 10) == 10 && !memcmp(h, "ID3", 3) && h[3] >= 2 && h[3] <= 4) {
        u32 size = syncsafe(h + 6);
        if (size > 0 && size <= MAX_COVER_TAG && (buf = malloc(size))) {
            len = sceIoRead(fd, buf, size);
            if (len != (int)size)
                len = 0;
        }
    }
    sceIoClose(fd);
    if (len <= 0) {
        free(buf);
        return NULL;
    }
    int ver = h[3], flags = h[5];
    if ((flags & 0x80) && ver < 4)
        len = unsync(buf, len);
    int pos = 0;
    if ((flags & 0x40) && ver >= 3 && len >= 4) { /* utökad header */
        u32 ext = ver == 3 ? be32(buf) : syncsafe(buf);
        pos = ext < (u32)len - 4 ? (int)ext + (ver == 3 ? 4 : 0) : len;
    }
    int idlen = ver == 2 ? 3 : 4, hdr = ver == 2 ? 6 : 10;
    int best = -1, best_len = 0, best_front = 0;
    while (pos + hdr <= len && buf[pos]) {
        unsigned char *fr = buf + pos;
        u32 fsize = ver == 2 ? ((u32)fr[3] << 16 | fr[4] << 8 | fr[5]) : ver == 4 ? syncsafe(fr + 4) : be32(fr + 4);
        if (fsize == 0 || fsize > (u32)(len - pos - hdr))
            break;
        if (!memcmp(fr, ver == 2 ? "PIC" : "APIC", idlen)) {
            unsigned char *d = fr + hdr;
            int dl = fsize;
            if (ver == 4 && (fr[9] & 0x01) && dl > 4) { /* längdfält före datat */
                d += 4;
                dl -= 4;
            }
            if (ver == 4 && (fr[9] & 0x02))
                dl = unsync(d, dl);
            /* kodning, MIME (v2.2: tre tecken), bildtyp, beskrivning – sedan bilden */
            int enc = d[0], p = 1;
            if (ver == 2) {
                p += 3;
            } else {
                while (p < dl && d[p])
                    p++;
                p++;
            }
            int type = p < dl ? d[p] : 0;
            p++;
            if (enc == 1 || enc == 2) { /* UTF-16: två nollbyte */
                while (p + 1 < dl && (d[p] || d[p + 1]))
                    p += 2;
                p += 2;
            } else {
                while (p < dl && d[p])
                    p++;
                p++;
            }
            if (p < dl && (best < 0 || (type == 3 && !best_front))) { /* typ 3 = framsida */
                best = (d - buf) + p;
                best_len = dl - p;
                best_front = type == 3;
            }
        }
        pos += hdr + fsize;
    }
    if (best < 0) {
        free(buf);
        return NULL;
    }
    memmove(buf, buf + best, best_len);
    *size_out = best_len;
    return buf;
}


/* ---------------------------------------------------------------------- */
/* the files that can be played, the folders of the Music screen */

static const Codec *codec_for(const char *name);

int music_supported(const char *name)
{
    return name && codec_for(name) != NULL;
}

static int name_is_hidden(const char *n)
{
    return n[0] == '.';
}

/* a case insensitive order for names */
static int cmp_text(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb || !ca)
            return ca - cb;
    }
}

static int cmp_entries(const void *a, const void *b)
{
    const MusicEntry *x = a, *y = b;
    if (x->dir != y->dir)
        return y->dir - x->dir; /* the folders first */
    int c = cmp_text(x->name, y->name);
    return c ? c : strcmp(x->name, y->name);
}

int music_list_dir(const char *dir, MusicEntry **out)
{
    return music_list_dir_filter(dir, music_supported, out);
}

int music_list_dir_filter(const char *dir, int (*ok)(const char *name), MusicEntry **out)
{
    *out = NULL;
    SceUID d = sceIoDopen(dir);
    if (d < 0)
        return -1;
    int cap = 64, n = 0;
    MusicEntry *e = malloc(cap * sizeof(MusicEntry));
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    while (e && n < MAX_ENTRIES && sceIoDread(d, &ent) > 0) {
        int isdir = FIO_S_ISDIR(ent.d_stat.st_mode) != 0;
        if (!name_is_hidden(ent.d_name) && strlen(ent.d_name) < sizeof(e[0].name) && (isdir || ok(ent.d_name))) {
            if (n >= cap) {
                MusicEntry *bigger = realloc(e, cap * 2 * sizeof(MusicEntry));
                if (!bigger)
                    break;
                e = bigger;
                cap *= 2;
            }
            snprintf(e[n].name, sizeof(e[n].name), "%s", ent.d_name);
            e[n].dir = isdir;
            n++;
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
    if (!e)
        return -1;
    if (n > 1)
        qsort(e, n, sizeof(MusicEntry), cmp_entries);
    *out = e;
    return n;
}

int music_roots(char roots[2][16])
{
    static const char *const R[2] = { "ms0:/MUSIC", "ef0:/MUSIC" };
    int n = 0;
    for (int i = 0; i < 2; i++) {
        if (i == 1 && sys_ms_is_ef())
            continue; /* ARK has put the internal storage at ms0: it is the same folder */
        SceUID d = sceIoDopen(R[i]);
        if (d < 0)
            continue;
        sceIoDclose(d);
        snprintf(roots[n++], 16, "%s", R[i]);
    }
    return n;
}

/* letters that the Latin font does not have (Cyrillic, Greek, kana, kanji ... : a UTF-8 lead byte of 0xCE or more) */
static int needs_cjk(const char *t)
{
    for (; *t; t++)
        if ((unsigned char)*t >= 0xCE)
            return 1;
    return 0;
}


/* ---------------------------------------------------------------------- */
/* the sound out (all of this runs in the player thread) */

static void src_release(void)
{
    if (!s_src_rate)
        return;
    /* the channel cannot be let go while the last buffer is playing: wait a little */
    for (int i = 0; i < 100 && sceAudioSRCChRelease() < 0; i++)
        sceKernelDelayThread(2000);
    s_src_rate = s_src_samples = 0;
}

static int src_reserve(int samples, int rate)
{
    if (s_src_rate == rate && s_src_samples == samples)
        return 1;
    src_release();
    if (sceAudioSRCChReserve(samples, rate, 2) < 0)
        return 0;
    s_src_rate = rate;
    s_src_samples = samples;
    return 1;
}


/* ---------------------------------------------------------------------- */
/* MP3 (sceMp3) */

static int s_handle = -1;
static int s_eof;
static int s_spf;                      /* samples per frame (1152 / 576), 0 = not known */
/* for jumping: where the stream starts and ends, the header of the first frame and the Xing table */
static u32 s_first_hdr;
static unsigned char s_toc[100];
static int s_has_toc;
static float s_seek_base;              /* seconds into the song where the decoder started */
static int s_seek_grace;               /* frames after a jump that may fail */

static void mp3_close(void)
{
    if (s_handle >= 0) {
        sceMp3ReleaseMp3Handle(s_handle);
        s_handle = -1;
    }
}

/* Fills the decoder's buffer from the file. <0 = a read error. */
static int feed(void)
{
    unsigned char *dst;
    SceInt32 towrite, srcpos;
    if (s_eof)
        return 0;
    if (sceMp3GetInfoToAddStreamData(s_handle, &dst, &towrite, &srcpos) < 0)
        return -1;
    if (towrite <= 0)
        return 0;
    int got = -1;
    if (sceIoLseek32(s_fd, srcpos, PSP_SEEK_SET) >= 0)
        got = sceIoRead(s_fd, dst, towrite);
    if (got < 0 && s_path[0]) {
        /* the file is closed by sleep mode: open it again and try once more */
        sceIoClose(s_fd);
        s_fd = sceIoOpen(s_path, PSP_O_RDONLY, 0777);
        if (s_fd >= 0 && sceIoLseek32(s_fd, srcpos, PSP_SEEK_SET) >= 0)
            got = sceIoRead(s_fd, dst, towrite);
    }
    if (got < 0)
        return -1;
    if (got == 0) {
        s_eof = 1;
        return 0;
    }
    return sceMp3NotifyAddStreamData(s_handle, got) < 0 ? -1 : 0;
}

static const int MP3_RATES[4][3] = { { 11025, 12000, 8000 }, { 0, 0, 0 }, { 22050, 24000, 16000 }, { 44100, 48000, 32000 } };

/* Bildrutans längd i byte ur huvudet (lager III), 0 = ogiltigt huvud */
static int frame_len(u32 h)
{
    static const short kbps1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
    static const short kbps2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
    int ver = (h >> 19) & 3, layer = (h >> 17) & 3, bi = (h >> 12) & 15, ri = (h >> 10) & 3;
    if ((h & 0xFFE00000) != 0xFFE00000 || ver == 1 || layer != 1 || ri == 3)
        return 0;
    int kbps = ver == 3 ? kbps1[bi] : kbps2[bi], rate = MP3_RATES[ver][ri];
    if (!kbps)
        return 0;
    return (ver == 3 ? 144 : 72) * kbps * 1000 / rate + ((h >> 9) & 1);
}

/* Läser första bildrutans huvud: prover per bildruta till s_spf, huvudet och Xing-tabellen
 * (för spolning), och längden från Xing/Info- eller VBRI-huvudet (VBR-filer).
 * Returnerar 0 om längden inte står där. */
static float parse_first_frame(u32 start)
{
    unsigned char b[192];
    s_first_hdr = 0;
    s_has_toc = 0;
    sceIoLseek32(s_fd, start, PSP_SEEK_SET);
    if (sceIoRead(s_fd, b, sizeof(b)) != sizeof(b))
        return 0;
    u32 h = be32(b);
    if (!frame_len(h))
        return 0;
    s_first_hdr = h;
    int ver = (h >> 19) & 3, ri = (h >> 10) & 3, mono = ((h >> 6) & 3) == 3;
    int rate = MP3_RATES[ver][ri], spf = ver == 3 ? 1152 : 576;
    s_spf = spf;
    int off = ver == 3 ? (mono ? 21 : 36) : (mono ? 13 : 21);
    u32 frames = 0;
    if (!memcmp(b + off, "Xing", 4) || !memcmp(b + off, "Info", 4)) {
        u32 flags = be32(b + off + 4);
        int p = off + 8;
        if (flags & 1) {
            frames = be32(b + p);
            p += 4;
        }
        if (flags & 2)
            p += 4; /* antal byte */
        if ((flags & 4) && p + 100 <= (int)sizeof(b)) {
            memcpy(s_toc, b + p, 100);
            s_has_toc = 1;
        }
    } else if (!memcmp(b + 36, "VBRI", 4)) {
        frames = be32(b + 36 + 14);
    }
    return frames && rate ? (float)frames * spf / rate : 0;
}

/* Startar avkodaren med strömmen från byte `offset` (början av en bildruta). */
static int start_decoder(u32 offset)
{
    if (s_handle >= 0) {
        sceMp3ReleaseMp3Handle(s_handle);
        s_handle = -1;
    }
    SceMp3InitArg args;
    memset(&args, 0, sizeof(args));
    args.mp3StreamStart = offset;
    args.mp3StreamEnd = s_size;
    args.mp3Buf = s_mp3buf;
    args.mp3BufSize = sizeof(s_mp3buf);
    args.pcmBuf = s_pcmbuf;
    args.pcmBufSize = sizeof(s_pcmbuf);
    s_handle = sceMp3ReserveMp3Handle(&args);
    if (s_handle < 0)
        return -1;
    s_eof = 0;
    if (feed() < 0 || sceMp3Init(s_handle) < 0) {
        sceMp3ReleaseMp3Handle(s_handle);
        s_handle = -1;
        return -1;
    }
    sceMp3SetLoopNum(s_handle, 0);
    return 0;
}

/* Opens the stream of the song that the player has open (s_fd, s_size, s_start): the decoder, the rate, the channels and the length */
static int mp3_open(void)
{
    if (start_decoder(s_start) < 0)
        return -1;
    s_rate = sceMp3GetSamplingRate(s_handle);
    s_channels = sceMp3GetMp3ChannelNum(s_handle);
    if (s_rate <= 0 || s_channels < 1 || s_channels > 2) {
        mp3_close();
        return -1;
    }
    s_spf = 0;
    float dur = parse_first_frame(s_start);
    if (dur <= 0) {
        int kbps = sceMp3GetBitRate(s_handle);
        dur = kbps > 0 ? (float)(s_size - s_start) * 8.0f / (kbps * 1000.0f) : 0;
    }
    s_duration = dur;
    s_decoded = s_seek_base = 0;
    s_seek_grace = 0;
    return 0;
}

/* Första giltiga bildrutan från byte `pos`: samma MPEG-version, lager och samplingsfrekvens
 * som låtens första bildruta, och en likadan bildruta direkt efter. Annars `pos`. */
static u32 find_frame(u32 pos)
{
    static unsigned char b[4096];
    sceIoLseek32(s_fd, pos, PSP_SEEK_SET);
    int n = sceIoRead(s_fd, b, sizeof(b));
    const u32 same = 0x001E0C00; /* version, lager, samplingsfrekvens */
    for (int i = 0; i + 4 <= n; i++) {
        u32 h = be32(b + i);
        int len = frame_len(h);
        if (!len || (s_first_hdr && ((h ^ s_first_hdr) & same)))
            continue;
        if (i + len + 4 <= n) {
            u32 next = be32(b + i + len);
            if (!frame_len(next) || ((next ^ h) & same))
                continue;
        }
        return pos + i;
    }
    return pos;
}

/* Hoppar till `sec` sekunder in i låten. 0 = klart, -1 = gick inte (låten är slut). */
static int mp3_seek(float sec)
{
    if (s_handle < 0 || s_fd < 0 || s_duration <= 0)
        return 0;
    if (sec >= s_duration - 0.5f)
        return -1;
    if (sec < 0)
        sec = 0;
    u32 span = s_size - s_start, pos;
    float f = sec / s_duration;
    if (s_has_toc) { /* VBR: Xing-tabellen säger var varje procent av låten börjar */
        float pct = f * 100;
        int i = pct < 99 ? (int)pct : 99;
        float a = s_toc[i], b = i < 99 ? s_toc[i + 1] : 256;
        pos = s_start + (u32)((a + (b - a) * (pct - i)) / 256.0f * span);
    } else {
        pos = s_start + (u32)(f * span);
    }
    u32 frame = sec > 0 ? find_frame(pos) : s_start;
    if (start_decoder(frame) < 0) { /* försök från början i stället */
        if (start_decoder(s_start) < 0)
            return -1;
        frame = s_start;
    }
    s_seek_base = frame == s_start ? 0 : sec;
    s_decoded = s_seek_base;
    /* bildrutorna efter ett hopp kan sakna data från bildrutan före (bitreservoaren) */
    s_seek_grace = 8;
    return 0;
}

/* Decodes one frame. 1 = a frame (or 0 samples: nothing to play this time), 0 = the song is over, -1 = an error */
static int mp3_frame(const short **pcm_out, int *samples_out, int *ch_out)
{
    if (sceMp3CheckStreamDataNeeded(s_handle) > 0 && feed() < 0)
        return -1;
    SceShort16 *pcm;
    int bytes = sceMp3Decode(s_handle, &pcm);
    /* just after a jump: skip the frame. The PSP answers MP3_END (not an error) to a frame that has no data from the frames before it (the bit reservoir),
     * so that must not count as the end of the song. */
    if (bytes <= 0 && s_seek_grace > 0) {
        s_seek_grace--;
        *samples_out = 0;
        return 1;
    }
    if (bytes == 0 || (u32)bytes == MP3_END)
        return 0;
    if (bytes < 0)
        return -1;
    /* the decoder may leave a mono file as stereo: decide from the size of the frame */
    int out_ch = s_channels;
    if (s_spf && bytes == s_spf * 4)
        out_ch = 2;
    else if (s_spf && bytes == s_spf * 2)
        out_ch = 1;
    else if (bytes / (2 * out_ch) > 4096)
        out_ch = 2;
    int samples = bytes / (2 * out_ch);
    if (samples <= 0 || samples > 4096)
        return -1;
    int sum = sceMp3GetSumDecodedSample(s_handle);
    if (sum >= 0 && s_rate > 0)
        s_decoded = s_seek_base + (float)sum / s_rate;
    if (s_seek_grace > 0)
        s_seek_grace--;
    *pcm_out = pcm;
    *samples_out = samples;
    *ch_out = out_ch;
    return 1;
}

static const Codec mp3_codec = { mp3_open, mp3_frame, mp3_seek, mp3_close };



/* ---------------------------------------------------------------------- */
/* WAV: plain PCM (8, 16, 24 or 32 bits, one or two channels), decoded here; the file is read a block at a time */

#define WAV_BLOCK 1024 /* samples per channel in a block */
static u32 s_wav_data, s_wav_len, s_wav_at; /* where the sound starts in the file, how long it is, how far it has been read (bytes of the sound) */
static int s_wav_bits, s_wav_ch, s_wav_align;
static unsigned char s_wav_raw[WAV_BLOCK * 2 * 4];
static short s_wav_out[WAV_BLOCK * 2];

static u32 le32(const unsigned char *p) { return p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u32 le16(const unsigned char *p) { return p[0] | ((u32)p[1] << 8); }

static int wav_open(void)
{
    unsigned char h[12], c[8], fmt[40];
    sceIoLseek32(s_fd, 0, PSP_SEEK_SET);
    if (sceIoRead(s_fd, h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
        return -1;
    int have_fmt = 0;
    u32 pos = 12;
    s_wav_data = s_wav_len = 0;
    while (pos + 8 <= s_size && sceIoLseek32(s_fd, pos, PSP_SEEK_SET) == (int)pos && sceIoRead(s_fd, c, 8) == 8) {
        u32 len = le32(c + 4);
        if (!memcmp(c, "fmt ", 4) && len >= 16) {
            int n = len < sizeof(fmt) ? (int)len : (int)sizeof(fmt);
            if (sceIoRead(s_fd, fmt, n) != n)
                return -1;
            u32 tag = le16(fmt);
            if (tag == 0xFFFE && n >= 26)
                tag = le16(fmt + 24); /* "extensible": the first two bytes of the sub-format are the real tag */
            if (tag != 1)
                return -1; /* not plain PCM (ADPCM, A-law, float, ATRAC3 in a wave file ...) */
            s_wav_ch = (int)le16(fmt + 2);
            s_rate = (int)le32(fmt + 4);
            s_wav_align = (int)le16(fmt + 12);
            s_wav_bits = (int)le16(fmt + 14);
            if (s_wav_ch < 1 || s_wav_ch > 2 || (s_wav_bits != 8 && s_wav_bits != 16 && s_wav_bits != 24 && s_wav_bits != 32) || s_wav_align != s_wav_ch * (s_wav_bits / 8))
                return -1;
            have_fmt = 1;
        } else if (!memcmp(c, "data", 4)) {
            if (!have_fmt)
                return -1; /* the data must come after the format */
            s_wav_data = pos + 8;
            s_wav_len = len;
            if (s_wav_data + s_wav_len > s_size || s_wav_len == 0xFFFFFFFFu)
                s_wav_len = s_size - s_wav_data; /* a stream or a cut file: what there is */
            s_wav_len -= s_wav_len % s_wav_align;
            break;
        }
        pos += 8 + len + (len & 1); /* chunks are padded to an even length */
    }
    if (!s_wav_data || s_wav_len < (u32)s_wav_align || s_rate <= 0)
        return -1;
    s_channels = s_wav_ch;
    s_wav_at = 0;
    s_duration = (float)s_wav_len / ((float)s_rate * s_wav_align);
    s_decoded = 0;
    return 0;
}

static int wav_frame(const short **pcm_out, int *samples_out, int *ch_out)
{
    if (s_wav_at >= s_wav_len)
        return 0;
    u32 want = (u32)WAV_BLOCK * s_wav_align;
    if (want > s_wav_len - s_wav_at)
        want = s_wav_len - s_wav_at;
    if (sceIoLseek32(s_fd, s_wav_data + s_wav_at, PSP_SEEK_SET) < 0)
        return -1;
    int got = sceIoRead(s_fd, s_wav_raw, want);
    if (got < 0 && s_path[0]) { /* the file is closed by sleep mode: open it again and try once more */
        sceIoClose(s_fd);
        s_fd = sceIoOpen(s_path, PSP_O_RDONLY, 0777);
        if (s_fd >= 0 && sceIoLseek32(s_fd, s_wav_data + s_wav_at, PSP_SEEK_SET) >= 0)
            got = sceIoRead(s_fd, s_wav_raw, want);
    }
    if (got <= 0)
        return got < 0 ? -1 : 0;
    got -= got % s_wav_align;
    int n = got / s_wav_align, values = n * s_wav_ch, step = s_wav_bits / 8;
    for (int i = 0; i < values; i++) {
        const unsigned char *p = s_wav_raw + i * step;
        s_wav_out[i] = s_wav_bits == 8 ? (short)(((int)p[0] - 128) * 256) : (short)((p[step - 1] << 8) | p[step - 2]); /* the top two bytes of 16, 24 and 32 bit samples */
    }
    s_wav_at += (u32)got;
    s_decoded = (float)s_wav_at / ((float)s_rate * s_wav_align);
    *pcm_out = s_wav_out;
    *samples_out = n;
    *ch_out = s_wav_ch;
    return 1;
}

static int wav_seek(float sec)
{
    if (sec >= s_duration - 0.5f)
        return -1;
    if (sec < 0)
        sec = 0;
    u32 at = (u32)((double)sec * s_rate) * (u32)s_wav_align;
    s_wav_at = at > s_wav_len ? s_wav_len : at;
    s_decoded = (float)s_wav_at / ((float)s_rate * s_wav_align);
    return 0;
}

static void wav_close(void) {}

static const Codec wav_codec = { wav_open, wav_frame, wav_seek, wav_close };

/* ---------------------------------------------------------------------- */
/* ATRAC3 and ATRAC3plus (the PSP's own music: .at3, .oma, .aa3), by the PSP's sceAtrac library. The file is read through a buffer of 64 KB: the library says where in
 * the file it wants more and where in the buffer it goes (sceAtracGetStreamDataInfo), and a jump asks it which part of the file it needs for the new place. */

#define ATRAC_BUF (64 * 1024)
static unsigned char __attribute__((aligned(64))) s_atrac_buf[ATRAC_BUF];
static short __attribute__((aligned(64))) s_atrac_out[2048 * 2];
static int s_atrac_id = -1, s_atrac_total, s_atrac_last, s_atrac_whole; /* whole = the file is all in the buffer: there is nothing to give it */
static int s_atrac_ready; /* the module is loaded */

static int atrac_read_at(unsigned char *dst, u32 offset, u32 bytes)
{
    int got = -1;
    if (sceIoLseek32(s_fd, offset, PSP_SEEK_SET) >= 0)
        got = sceIoRead(s_fd, dst, bytes);
    if (got < 0 && s_path[0]) { /* the file is closed by sleep mode: open it again and try once more */
        sceIoClose(s_fd);
        s_fd = sceIoOpen(s_path, PSP_O_RDONLY, 0777);
        if (s_fd >= 0 && sceIoLseek32(s_fd, offset, PSP_SEEK_SET) >= 0)
            got = sceIoRead(s_fd, dst, bytes);
    }
    return got;
}

/* gives the decoder the part of the file that it asks for. <0 = an error. */
static int atrac_feed(void)
{
    u8 *to;
    u32 room, from;
    if (s_atrac_whole)
        return 0;
    if (sceAtracGetStreamDataInfo(s_atrac_id, &to, &room, &from) < 0)
        return -1;
    if (room == 0)
        return 0;
    int got = atrac_read_at(to, from, room);
    if (got < 0)
        return -1;
    if (got > 0 && sceAtracAddStreamData(s_atrac_id, (unsigned)got) < 0)
        return -1;
    return 0;
}

static void atrac_close(void)
{
    if (s_atrac_id >= 0) {
        sceAtracReleaseAtracID(s_atrac_id);
        s_atrac_id = -1;
    }
}

static int atrac_open(void)
{
    int got = atrac_read_at(s_atrac_buf, 0, ATRAC_BUF); /* (from the start of the file, with the header: not after the tag) */
    if (got < 16)
        return -1;
    s_atrac_whole = s_size <= (u32)got;
    s_atrac_id = s_atrac_whole ? sceAtracSetDataAndGetID(s_atrac_buf, got) : sceAtracSetHalfwayBufferAndGetID(s_atrac_buf, got, ATRAC_BUF);
    if (s_atrac_id < 0)
        return -1;
    u32 ch = 2;
    int end = 0, loop_a = 0, loop_b = 0;
    if (sceAtracGetChannel(s_atrac_id, &ch) < 0 || sceAtracGetSoundSample(s_atrac_id, &end, &loop_a, &loop_b) < 0 || ch < 1 || ch > 2 || end <= 0) {
        atrac_close();
        return -1;
    }
    sceAtracSetLoopNum(s_atrac_id, 0); /* plays once: the player decides what comes next */
    s_rate = 44100;
    s_channels = (int)ch;
    s_atrac_total = end;
    s_atrac_last = 0;
    s_duration = (float)end / s_rate;
    s_decoded = 0;
    return 0;
}

static int atrac_frame(const short **pcm_out, int *samples_out, int *ch_out)
{
    if (s_atrac_last)
        return 0;
    if (atrac_feed() < 0)
        return -1;
    int n = 0, end = 0, remain = 0;
    int r = sceAtracDecodeData(s_atrac_id, (u16 *)s_atrac_out, &n, &end, &remain);
    if (r < 0) {
        if ((u32)r == PSP_ATRAC_ERROR_ALLDATA_WAS_DECODED)
            return 0;
        if ((u32)r == PSP_ATRAC_ERROR_NODATA_IN_BUFFER) { /* it wants more of the file before it can go on */
            *samples_out = 0;
            return atrac_feed() < 0 ? -1 : 1;
        }
        return -1;
    }
    u32 at = 0;
    if (sceAtracGetNextDecodePosition(s_atrac_id, &at) >= 0)
        s_decoded = (float)at / s_rate;
    else
        s_decoded += (float)n / s_rate;
    if (end)
        s_atrac_last = 1; /* the last frame is played, the next call says the song is over */
    if (n <= 0)
        return end ? 0 : 1;
    *pcm_out = s_atrac_out;
    *samples_out = n;
    *ch_out = 2; /* the library gives two channels (a mono file twice) unless it is told otherwise */
    return 1;
}

static int atrac_seek(float sec)
{
    int sample = (int)((double)sec * s_rate);
    if (sample >= s_atrac_total - s_rate / 2)
        return -1;
    if (sample < 0)
        sample = 0;
    PspBufferInfo bi;
    memset(&bi, 0, sizeof(bi));
    if (sceAtracGetBufferInfoForReseting(s_atrac_id, (u32)sample, &bi) < 0)
        return 0; /* it will not jump: the song goes on where it was */
    int got = atrac_read_at(bi.pucWritePositionFirstBuf, bi.uiReadPositionFirstBuf, bi.uiWritableByteFirstBuf);
    if (got < 0 || sceAtracResetPlayPosition(s_atrac_id, (u32)sample, (u32)got, 0) < 0)
        return 0;
    s_atrac_last = 0;
    s_decoded = (float)sample / s_rate;
    return 0;
}

static const Codec atrac_codec = { atrac_open, atrac_frame, atrac_seek, atrac_close };

/* ---------------------------------------------------------------------- */
/* the kinds of song file: by their ending */

static const struct { const char *ext; const Codec *codec; } FORMATS[] = {
    { "mp3", &mp3_codec },
    { "wav", &wav_codec },
    { "at3", &atrac_codec },
    { "oma", &atrac_codec },
    { "aa3", &atrac_codec },
};

static const Codec *codec_for(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot || !dot[1])
        return NULL;
    for (unsigned i = 0; i < sizeof(FORMATS) / sizeof(FORMATS[0]); i++)
        if (!strcasecmp(dot + 1, FORMATS[i].ext))
            return FORMATS[i].codec == &atrac_codec && !s_atrac_ready ? NULL : FORMATS[i].codec; /* (ATRAC only when its module is loaded) */
    return NULL;
}

/* the start of the stream: after an ID3v2 tag, if the file has one */
static u32 stream_start(void)
{
    u32 s = id3v2_size(s_fd);
    return s >= s_size ? 0 : s;
}

/* the title of a file with no tag: its name without the ending */
static void title_from_name(const char *path, char *out, int size)
{
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *dot = strrchr(name, '.');
    int n = dot && dot != name ? (int)(dot - name) : (int)strlen(name);
    snprintf(out, size, "%.*s", n, name);
}

/* the tags of the song at `path`, into *t (the title is the file's name when there is none) */
static void read_song_tags(const char *path, Track *t)
{
    Item it;
    memset(&it, 0, sizeof(it));
    snprintf(it.t.path, sizeof(it.t.path), "%s", path);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    unsigned char *buf = fd >= 0 ? malloc(TAG_READ) : NULL;
    if (buf) {
        u32 size = sceIoLseek32(fd, 0, PSP_SEEK_END);
        read_tags(fd, size, &it, buf);
    }
    free(buf);
    if (fd >= 0)
        sceIoClose(fd);
    if (!it.t.title[0])
        title_from_name(path, it.t.title, sizeof(it.t.title));
    *t = it.t;
}

static void publish_track(const Track *t)
{
    s_now_seq++; /* odd: being written */
    s_now = *t;
    s_now_valid = 1;
    s_cjk = needs_cjk(t->title) || needs_cjk(t->artist) || needs_cjk(t->album);
    s_gen++;
    s_now_seq++;
}

static void close_track(void)
{
    if (s_codec)
        s_codec->close();
    s_codec = NULL;
    if (s_fd >= 0) {
        sceIoClose(s_fd);
        s_fd = -1;
    }
    s_path[0] = 0;
}

/* the rates that the SRC channel takes (pspaudio.h) */
static int rate_ok(int rate)
{
    static const int R[] = { 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 11050, 8000 };
    for (unsigned i = 0; i < sizeof(R) / sizeof(R[0]); i++)
        if (rate == R[i])
            return 1;
    return 0;
}

/* Opens the song at `path` (the file, the codec, its tags): 0 = ready to play */
static int open_track(const char *path)
{
    close_track();
    const Codec *c = codec_for(path);
    if (!c)
        return -1;
    s_fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (s_fd < 0)
        return -1;
    snprintf(s_path, sizeof(s_path), "%s", path);
    s_size = sceIoLseek32(s_fd, 0, PSP_SEEK_END);
    s_start = stream_start();
    s_codec = c;
    s_decoded = 0;
    if (c->open() < 0 || !rate_ok(s_rate)) {
        close_track();
        return -1;
    }
    s_elapsed = 0;
    Track t;
    read_song_tags(path, &t);
    publish_track(&t);
    return 0;
}

/* Decodes a frame and plays it. 1 = go on, 0 = the song is over, -1 = an error */
static int play_frame(void)
{
    const short *pcm = NULL;
    int samples = 0, ch = 2;
    int r = s_codec->frame(&pcm, &samples, &ch);
    if (r <= 0)
        return r;
    if (samples == 0)
        return 1; /* nothing this time (a frame skipped after a jump) */
    if (samples < 17)
        return 0; /* the channel does not take fewer: a last scrap of a song is the end of it */
    if (samples > MAX_SAMPLES || !src_reserve(samples, s_rate))
        return -1; /* the channel takes 17 to 4111 samples a call; it is reserved for the size of the frames, which is the same for every frame of a song */
    const void *out = pcm;
    if (ch == 1) { /* mono becomes stereo (the output wants two channels) */
        short *o = s_out[s_out_idx];
        s_out_idx ^= 1;
        for (int i = 0; i < samples; i++)
            o[i * 2] = o[i * 2 + 1] = pcm[i];
        out = o;
    }
    sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, (void *)out);
    s_elapsed = s_decoded;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* the queue: the songs of the folder, in order or shuffled */

typedef struct {
    char dir[256];
    int n;
    char (*name)[QNAME];
} Queue;

static Queue s_queue;                    /* the songs that play (the player thread's) */
static Queue s_stage;                    /* a new folder that the screen has made ready, not taken by the thread yet */
static volatile int s_stage_ready;
static SceUID s_lock = -1;
static unsigned short s_order[MAX_ENTRIES]; /* the order the songs play in: s_order[s_pos] is the one that plays now */
static int s_pos;
static unsigned s_rng = 0x1d872b41u;

static void lock(void)
{
    if (s_lock >= 0)
        sceKernelWaitSema(s_lock, 1, NULL);
}

static void unlock(void)
{
    if (s_lock >= 0)
        sceKernelSignalSema(s_lock, 1);
}

static unsigned rnd(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return s_rng >> 8;
}

static void queue_free(Queue *q)
{
    free(q->name);
    q->name = NULL;
    q->n = 0;
}

/* The order of the songs: in order, or shuffled with `first` at the front. s_pos is where `first` is (a song that is not given: the first of the order). */
static void build_order(int first)
{
    int n = s_queue.n;
    for (int i = 0; i < n; i++)
        s_order[i] = i;
    s_pos = 0;
    if (s_shuffle && n > 1) {
        for (int i = n - 1; i > 0; i--) {
            int j = rnd() % (i + 1);
            unsigned short t = s_order[i];
            s_order[i] = s_order[j];
            s_order[j] = t;
        }
        if (first >= 0 && first < n) {
            for (int i = 0; i < n; i++)
                if (s_order[i] == first) {
                    s_order[i] = s_order[0];
                    s_order[0] = first;
                    break;
                }
        }
    } else if (first >= 0 && first < n) {
        s_pos = first;
    }
}

static void song_path(int queue_index, char *out, int size)
{
    snprintf(out, size, "%s/%s", s_queue.dir, s_queue.name[queue_index]);
}

/* the place after `pos` going `step` (1 or -1), -1 = there is none: at the end of the folder it goes round when it repeats all (a shuffled folder is shuffled again for the next round) */
static int step_pos(int pos, int step)
{
    int n = s_queue.n, p = pos + step;
    if (n <= 0)
        return -1;
    if (p >= n) {
        if (s_repeat != MUSIC_REPEAT_ALL)
            return -1;
        build_order(-1);
        return 0;
    }
    if (p < 0)
        return s_repeat == MUSIC_REPEAT_ALL ? n - 1 : -1;
    return p;
}

/* Starts the song at place `pos` of the order; a file that cannot be played is skipped, going on in the direction `step` (1 = on, -1 = back, for the button that goes to the song before). */
static void start_at(int pos, int step)
{
    int n = s_queue.n;
    for (int tries = 0; tries < n && pos >= 0 && pos < n; tries++) {
        if (s_quit || (tries > 0 && command_waiting()))
            break; /* psp5 is closing, or the user chose something else */
        char path[256];
        song_path(s_order[pos], path, sizeof(path));
        if (open_track(path) == 0) {
            s_pos = pos;
            s_state = MUSIC_PLAYING;
            return;
        }
        pos = step_pos(pos, step);
    }
    close_track();
    src_release();
    s_now_valid = 0;
    s_state = MUSIC_STOPPED;
    s_elapsed = s_duration = 0;
}

static void stop_playback(void)
{
    close_track();
    src_release();
    s_now_valid = 0;
    s_state = MUSIC_STOPPED;
    s_elapsed = s_duration = 0;
}

/* the song after (dir = 1) or before (dir = -1) this one, -1 = there is none (the end of the folder when it does not repeat) */
static int neighbour(int dir)
{
    return step_pos(s_pos, dir);
}

static void go_next(void)
{
    int p = neighbour(1);
    if (p >= 0)
        start_at(p, 1);
    else
        stop_playback();
}

/* the song is over by itself: the same again when it repeats, else the next */
static void song_over(void)
{
    if (s_repeat == MUSIC_REPEAT_ONE)
        start_at(s_pos, 1);
    else
        go_next();
}

static void handle_command(int cmd, int arg)
{
    switch (cmd) {
    case CMD_LOAD: { /* the folder the screen has made ready; arg = the song to start with */
        lock();
        if (s_stage_ready) {
            queue_free(&s_queue);
            s_queue = s_stage;
            s_stage.name = NULL;
            s_stage.n = 0;
            s_stage_ready = 0;
        }
        unlock();
        snprintf(s_dir_pub, sizeof(s_dir_pub), "%s", s_queue.dir);
        build_order(arg);
        start_at(s_pos, 1);
        break;
    }
    case CMD_PAUSE:
        if (s_state == MUSIC_PLAYING)
            s_state = MUSIC_PAUSED;
        else if (s_state == MUSIC_PAUSED)
            s_state = MUSIC_PLAYING;
        break;
    case CMD_STOP:
        stop_playback();
        break;
    case CMD_NEXT:
        if (s_state != MUSIC_STOPPED)
            go_next();
        break;
    case CMD_PREV:
        if (s_state == MUSIC_STOPPED)
            break;
    {
        int p = s_elapsed > 3.0f ? -1 : neighbour(-1);
        if (p < 0)
            start_at(s_pos, 1);
        else
            start_at(p, -1);
        break;
    }
    case CMD_SEEK: /* arg = milliseconds; past the end = the next song */
        if (s_state != MUSIC_STOPPED && s_codec && s_codec->seek(arg / 1000.0f) < 0)
            song_over();
        else if (s_state != MUSIC_STOPPED && s_codec)
            s_elapsed = s_decoded;
        break;
    case CMD_SHUFFLE: { /* the song that plays goes on, the order changes round it */
        int cur = s_queue.n > 0 && s_pos < s_queue.n ? s_order[s_pos] : -1;
        s_shuffle = arg != 0;
        build_order(cur);
        break;
    }
    }
}

static int player_thread(SceSize args, void *argp)
{
    sceKernelChangeThreadPriority(0, 0x14);
    int failures = 0;
    while (!s_quit) {
        while (s_q_tail != s_q_head) {
            int t = s_q_tail;
            s_in_cmd = 1;
            handle_command(s_q_cmd[t], s_q_arg[t]);
            s_in_cmd = 0;
            s_q_tail = (t + 1) % QUEUE;
            failures = 0;
        }
        if (s_state != MUSIC_PLAYING || !s_codec) {
            sceKernelDelayThread(10000);
            continue;
        }
        int r = play_frame();
        if (r > 0) {
            failures = 0;
            continue;
        }
        /* over, or an error: the next song (after the last: stop) */
        if (r < 0 && ++failures >= (s_queue.n < 8 ? s_queue.n : 8)) {
            stop_playback();
            continue;
        }
        song_over();
    }
    close_track();
    src_release();
    return 0;
}

static void push(int cmd, int arg)
{
    if (s_thread < 0)
        return;
    int h = s_q_head, n = (h + 1) % QUEUE;
    if (n == s_q_tail)
        return; /* the queue is full: the command is dropped */
    s_q_cmd[h] = cmd;
    s_q_arg[h] = arg;
    s_q_head = n;
}

/* ---------------------------------------------------------------------- */
/* life cycle and control */

void music_init(void)
{
    if (s_thread >= 0)
        return;
    /* without a given SDK version sceMp3 only accepts 44.1 kHz (48 kHz and the others are refused) */
    if (sceKernelGetCompiledSdkVersion() == 0)
        sceKernelSetCompiledSdkVersion(0x06060010);
    int r = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    s_loaded_avcodec = r >= 0;
    if (r < 0 && (u32)r != ALREADY_LOADED && (u32)r != ALREADY_LOADED2)
        return;
    r = sceUtilityLoadModule(PSP_MODULE_AV_MP3);
    s_loaded_mp3 = r >= 0;
    if (r < 0 && (u32)r != ALREADY_LOADED && (u32)r != ALREADY_LOADED2) {
        if (s_loaded_avcodec)
            sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
        s_loaded_avcodec = 0;
        return;
    }
    if (sceMp3InitResource() < 0) {
        if (s_loaded_mp3)
            sceUtilityUnloadModule(PSP_MODULE_AV_MP3);
        if (s_loaded_avcodec)
            sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
        s_loaded_mp3 = s_loaded_avcodec = 0;
        return;
    }
    s_ready = 1;
    r = sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS); /* ATRAC3 and ATRAC3plus: if it cannot be loaded those files are not offered, the rest is */
    s_loaded_atrac = r >= 0;
    s_atrac_ready = r >= 0 || (u32)r == ALREADY_LOADED || (u32)r == ALREADY_LOADED2;
    s_quit = 0;
    s_q_head = s_q_tail = 0;
    s_state = MUSIC_STOPPED;
    s_now_valid = 0;
    s_elapsed = s_duration = 0;
    s_lock = sceKernelCreateSema("psp5_music", 0, 1, 1, NULL);
    s_thread = sceKernelCreateThread("psp5_music", player_thread, 0x14, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (s_thread >= 0 && sceKernelStartThread(s_thread, 0, NULL) < 0) {
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
}

void music_term(void)
{
    if (s_thread >= 0) {
        s_quit = 1;
        sceKernelWaitThreadEnd(s_thread, NULL);
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
    close_track();
    src_release();
    if (s_ready) {
        sceMp3TermResource();
        s_ready = 0;
    }
    if (s_loaded_atrac)
        sceUtilityUnloadModule(PSP_MODULE_AV_ATRAC3PLUS);
    if (s_loaded_mp3)
        sceUtilityUnloadModule(PSP_MODULE_AV_MP3);
    if (s_loaded_avcodec)
        sceUtilityUnloadModule(PSP_MODULE_AV_AVCODEC);
    s_loaded_atrac = s_loaded_mp3 = s_loaded_avcodec = 0;
    s_atrac_ready = 0;
    queue_free(&s_queue);
    queue_free(&s_stage);
    s_stage_ready = 0;
    if (s_lock >= 0) {
        sceKernelDeleteSema(s_lock);
        s_lock = -1;
    }
    s_dir_pub[0] = 0;
    s_now_valid = 0;
    s_cjk = 0;
    s_gen++;
    s_state = MUSIC_STOPPED;
    s_elapsed = s_duration = 0;
}

void music_play_folder(const char *dir, const MusicEntry *entries, int n, int index)
{
    if (s_thread < 0 || !dir || !entries || index < 0 || index >= n || entries[index].dir)
        return;
    int songs = 0, start = 0;
    for (int i = 0; i < n; i++)
        if (!entries[i].dir) {
            if (i == index)
                start = songs;
            songs++;
        }
    if (songs <= 0 || songs > MAX_ENTRIES)
        return;
    char (*names)[QNAME] = malloc((size_t)songs * QNAME);
    if (!names)
        return;
    int k = 0;
    for (int i = 0; i < n; i++)
        if (!entries[i].dir)
            snprintf(names[k++], QNAME, "%s", entries[i].name);
    lock();
    queue_free(&s_stage); /* one that the thread has not taken yet is replaced */
    snprintf(s_stage.dir, sizeof(s_stage.dir), "%s", dir);
    s_stage.name = names;
    s_stage.n = songs;
    s_stage_ready = 1;
    unlock();
    push(CMD_LOAD, start);
}

void music_toggle_pause(void) { push(CMD_PAUSE, 0); }
void music_stop(void)         { push(CMD_STOP, 0); }
void music_next(void)         { push(CMD_NEXT, 0); }
void music_prev(void)         { push(CMD_PREV, 0); }
void music_seek(float sec)    { push(CMD_SEEK, (int)(sec * 1000)); }

void music_set_shuffle(int on)
{
    s_shuffle = on != 0; /* (the thread builds the order again at once: a song that starts before that comes in the old order) */
    push(CMD_SHUFFLE, on != 0);
}

int music_shuffle(void) { return s_shuffle; }

void music_set_repeat(int mode)
{
    s_repeat = mode >= MUSIC_REPEAT_OFF && mode <= MUSIC_REPEAT_ALL ? mode : MUSIC_REPEAT_OFF;
}

int music_repeat(void) { return s_repeat; }

int music_generation(void) { return s_gen; }
int music_needs_cjk(void)  { return s_cjk; }
int music_state(void)      { return s_state; }
float music_elapsed(void)  { return s_elapsed; }
float music_duration(void) { return s_duration; }
const char *music_folder(void) { return s_dir_pub; }

int music_track(Track *out)
{
    for (int tries = 0; tries < 4; tries++) {
        int seq = s_now_seq;
        if (seq & 1)
            continue;
        if (!s_now_valid)
            return 0;
        *out = s_now;
        if (seq == s_now_seq)
            return 1; /* nothing was written while it was copied */
    }
    return 0;
}

int music_is_playing_file(const char *dir, const char *name)
{
    if (s_state == MUSIC_STOPPED || !s_now_valid)
        return 0;
    Track t;
    if (!music_track(&t))
        return 0;
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    return !strcmp(path, t.path);
}
