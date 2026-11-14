/* psp5 authentic – an AAC file in ADTS form (see adts.h) */
#include "adts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FRAME 8192
#define SEARCH 65536 /* how far a frame is looked for after a damaged place */

static const int RATES[13] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };

struct Adts {
    FILE *f;
    char path[256];
    long start, size;   /* where the stream starts (after an ID3v2 tag) and the size of the file */
    long pos;           /* where the next frame starts */
    long fpos;          /* where the file is (-1: not known): a read right after the one before needs no seek */
    long frame_no;      /* the number of the next frame (an estimate after a jump) */
    int rate, channels, sfi;
    unsigned char cfg[2];
    double avg;         /* the average length of a frame (bytes) */
    float duration;
};

/* the header at h (9 bytes are there): the length of the frame and of its header, 0 when it is none of ours */
static int parse(const unsigned char *h, int *hdr_len, int *sfi, int *channels)
{
    if (h[0] != 0xFF || (h[1] & 0xF6) != 0xF0) /* the 12 bits of sync and layer 00 */
        return 0;
    int profile = h[2] >> 6;
    int fi = (h[2] >> 2) & 15;
    int ch = ((h[2] & 1) << 2) | (h[3] >> 6);
    int len = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5);
    int blocks = (h[6] & 3) + 1;
    if (profile != 1 || fi >= 13 || ch < 1 || ch > 7 || blocks != 1) /* LC only, the channel configurations 1 to 7 (0, a channel layout in the stream, is not read), one block to a frame */
        return 0;
    int hl = (h[1] & 1) ? 7 : 9;
    if (len < hl + 1 || len > MAX_FRAME)
        return 0;
    *hdr_len = hl;
    *sfi = fi;
    *channels = ch;
    return len;
}

static int seek_to(Adts *a, long at)
{
    if (a->fpos == at)
        return 0;
    if (fseek(a->f, at, SEEK_SET)) {
        a->fpos = -1;
        return -1;
    }
    a->fpos = at;
    return 0;
}

static int rd(Adts *a, void *p, size_t n)
{
    if (fread(p, 1, n, a->f) != n) {
        a->fpos = -1;
        return 0;
    }
    if (a->fpos >= 0)
        a->fpos += (long)n;
    return 1;
}

/* the header at `at`, read and understood: the length of the frame (0 when there is none of ours there), the length of the header, the rate number and the channels */
static int read_header(Adts *a, long at, int *hl, int *sfi, int *ch)
{
    unsigned char h[9];
    if (at < 0 || at + 7 > a->size || seek_to(a, at) || !rd(a, h, 7))
        return 0;
    if (!(h[1] & 1) && (at + 9 > a->size || !rd(a, h + 7, 2)))
        return 0;
    return parse(h, hl, sfi, ch);
}

/* 1 when a frame starts at `at` and (unless it is the last) another one of the same kind right after it: a place that only looks like a header is not taken */
static int frame_at(Adts *a, long at, int *len, int *hdr_len, int *sfi, int *ch)
{
    int l = read_header(a, at, hdr_len, sfi, ch);
    if (!l || at + l > a->size)
        return 0;
    if (at + l + 7 <= a->size) {
        int hl2, sf2, ch2;
        if (!read_header(a, at + l, &hl2, &sf2, &ch2) || sf2 != *sfi || ch2 != *ch)
            return 0;
    }
    *len = l;
    return 1;
}

/* the first frame at or after `from` (within SEARCH bytes): its place, or -1 */
static long find_frame(Adts *a, long from)
{
    unsigned char buf[4096];
    long end = from + SEARCH < a->size ? from + SEARCH : a->size;
    for (long at = from; at < end;) {
        long want = end - at < (long)sizeof(buf) ? end - at : (long)sizeof(buf);
        if (seek_to(a, at))
            return -1;
        long got = (long)fread(buf, 1, (size_t)want, a->f);
        a->fpos = got > 0 ? at + got : -1;
        if (got <= 0)
            return -1;
        for (long i = 0; i < got; i++) {
            if (buf[i] != 0xFF)
                continue;
            int len, hl, sfi, ch;
            if (frame_at(a, at + i, &len, &hl, &sfi, &ch) && (a->rate == 0 || (sfi == a->sfi && ch == a->channels)))
                return at + i;
        }
        at += got;
    }
    return -1;
}

static void setup_reader(Adts *a)
{
    setvbuf(a->f, NULL, _IOFBF, 16384);
    a->fpos = 0;
}

Adts *adts_open(const char *path)
{
    Adts *a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    snprintf(a->path, sizeof(a->path), "%s", path);
    a->f = fopen(path, "rb");
    if (!a->f) {
        free(a);
        return NULL;
    }
    setup_reader(a);
    fseek(a->f, 0, SEEK_END);
    a->size = ftell(a->f);
    unsigned char h[10];
    long from = 0;
    fseek(a->f, 0, SEEK_SET);
    a->fpos = 0;
    if (rd(a, h, 10) && !memcmp(h, "ID3", 3) && h[3] >= 2 && h[3] <= 4) { /* an ID3v2 tag in front (and its footer, when it has one) */
        from = 10 + (long)(((h[6] & 0x7F) << 21) | ((h[7] & 0x7F) << 14) | ((h[8] & 0x7F) << 7) | (h[9] & 0x7F));
        if (h[3] == 4 && (h[5] & 0x10))
            from += 10;
    }
    long first = find_frame(a, from);
    int len, hl, sfi, ch;
    if (first < 0 || !frame_at(a, first, &len, &hl, &sfi, &ch)) {
        adts_close(a);
        return NULL;
    }
    a->start = first;
    a->pos = first;
    a->sfi = sfi;
    a->channels = ch;
    a->rate = RATES[sfi];
    a->cfg[0] = (unsigned char)((2 << 3) | (sfi >> 1)); /* the AudioSpecificConfig: object type 2 (LC), the rate, the channels */
    a->cfg[1] = (unsigned char)(((sfi & 1) << 7) | (ch << 3));
    /* the average frame: over the first 64 frames */
    long at = first;
    int n = 0;
    while (n < 64 && frame_at(a, at, &len, &hl, &sfi, &ch)) {
        at += len;
        n++;
    }
    a->avg = n ? (double)(at - first) / n : 0;
    if (a->avg < 8) {
        adts_close(a);
        return NULL;
    }
    a->duration = (float)(((double)(a->size - first) / a->avg) * 1024.0 / a->rate);
    return a;
}

void adts_close(Adts *a)
{
    if (!a)
        return;
    if (a->f)
        fclose(a->f);
    free(a);
}

int adts_reopen(Adts *a)
{
    if (a->f)
        fclose(a->f);
    a->f = fopen(a->path, "rb");
    if (!a->f)
        return 0;
    setup_reader(a);
    return 1;
}

int adts_rate(const Adts *a) { return a->rate; }
int adts_channels(const Adts *a) { return a->channels; }
const unsigned char *adts_config(const Adts *a) { return a->cfg; }
float adts_duration(const Adts *a) { return a->duration; }
float adts_time(const Adts *a) { return (float)((double)a->frame_no * 1024.0 / a->rate); }

int adts_next(Adts *a, unsigned char *buf, int cap)
{
    if (!a->f)
        return -1;
    int hl = 0, sfi = 0, ch = 0;
    int len = read_header(a, a->pos, &hl, &sfi, &ch);
    if (!len || sfi != a->sfi || ch != a->channels || a->pos + len > a->size) { /* not a frame of this stream where one is due */
        if (a->pos + 7 > a->size)
            return 0; /* the end */
        if (a->fpos < 0 && ferror(a->f))
            return -1;
        long next = find_frame(a, a->pos + 1); /* a damaged place: on at the next frame there is */
        if (next < 0)
            return ferror(a->f) ? -1 : 0;
        a->pos = next;
        int l2, hl2, sf2, ch2;
        if (!frame_at(a, a->pos, &l2, &hl2, &sf2, &ch2))
            return 0;
        len = l2;
        hl = hl2;
        if (seek_to(a, a->pos + hl))
            return -1;
    }
    int body = len - hl;
    if (body > cap)
        return -1;
    if (!rd(a, buf, (size_t)body))
        return -1;
    a->pos += len;
    a->frame_no++;
    return body;
}

int adts_seek(Adts *a, float sec)
{
    if (sec < 0)
        sec = 0;
    if (sec >= a->duration - 0.2f)
        return -1;
    long frames = (long)((double)sec * a->rate / 1024.0);
    long at = a->start + (long)((double)frames * a->avg);
    if (at >= a->size)
        return -1;
    if (at < a->start)
        at = a->start;
    long f = at == a->start ? at : find_frame(a, at);
    if (f < 0)
        return -1;
    a->pos = f;
    a->frame_no = frames;
    return 0;
}
