/* psp5 authentic – Annex B, ADTS and the MPEG program stream writer (see mpegps.h) */
#include <string.h>
#include "mpegps.h"

int mpegps_annexb(const uint8_t *s, int len, int nal_size, const uint8_t *params, int params_len, int key, uint8_t *out, int cap)
{
    static const uint8_t aud[6] = { 0, 0, 0, 1, 0x09, 0xF0 }; /* "any slice type" */
    int n = 0;
    if (nal_size < 1 || nal_size > 4 || len < nal_size)
        return -1;
    int first_type = -1;
    {
        int l = 0;
        for (int i = 0; i < nal_size; i++)
            l = (l << 8) | s[i];
        if (l > 0 && nal_size + l <= len)
            first_type = s[nal_size] & 0x1F;
    }
    if (first_type != 9) {
        if (cap < 6)
            return -1;
        memcpy(out, aud, 6);
        n = 6;
    }
    if (key && params_len > 0) {
        if (n + params_len > cap)
            return -1;
        memcpy(out + n, params, (size_t)params_len);
        n += params_len;
    }
    int pos = 0;
    while (pos + nal_size <= len) {
        int l = 0;
        for (int i = 0; i < nal_size; i++)
            l = (l << 8) | s[pos + i];
        pos += nal_size;
        if (l <= 0 || pos + l > len)
            return -1;
        if (n + 4 + l > cap)
            return -1;
        out[n++] = 0;
        out[n++] = 0;
        out[n++] = 0;
        out[n++] = 1;
        memcpy(out + n, s + pos, (size_t)l);
        n += l;
        pos += l;
    }
    return pos == len ? n : -1;
}

int mpegps_adts(const uint8_t *asc, int asc_len, int payload, uint8_t out[7])
{
    static const int RATES[13] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    (void)RATES;
    if (asc_len < 2)
        return 0;
    int obj = asc[0] >> 3;
    int freq = ((asc[0] & 7) << 1) | (asc[1] >> 7);
    int chan = (asc[1] >> 3) & 15;
    if (freq == 15 || freq > 12 || obj < 1 || obj > 4 || chan < 1 || chan > 7)
        return 0;
    int total = payload + 7;
    out[0] = 0xFF;
    out[1] = 0xF1; /* MPEG-4, no CRC */
    out[2] = (uint8_t)(((obj - 1) << 6) | (freq << 2) | (chan >> 2));
    out[3] = (uint8_t)(((chan & 3) << 6) | ((total >> 11) & 3));
    out[4] = (uint8_t)((total >> 3) & 0xFF);
    out[5] = (uint8_t)(((total & 7) << 5) | 0x1F);
    out[6] = 0xFC;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* the program stream */

void ps_init(PsWriter *w)
{
    memset(w, 0, sizeof(*w));
}

void ps_au(PsWriter *w, const uint8_t *annexb, int len, int64_t pts, int64_t dts)
{
    w->data = annexb;
    w->len = len;
    w->pos = 0;
    w->pts = pts < 0 ? 0 : pts;
    /* the PSP's own streams have the decoding time at least one picture (3003 ticks) before the time to show it, also when nothing is reordered */
    w->dts = dts > w->pts - 3003 ? w->pts - 3003 : dts;
    if (w->dts < 0)
        w->dts = 0;
    w->first = 1;
}

int ps_pending(const PsWriter *w)
{
    return w->len - w->pos;
}

/* what is in a sector: the pack header, [system header, the table packet], then one PES packet. The first sector of a unit has the time stamps (always both) and the P-STD extension of the PSP's own streams */
#define PS_PACK 14
#define PS_SYSTEM 18
#define PS_PES_FIXED 9
#define PS_STAMPS 13
#define PS_TABLE_PACKET(n) (6 + 18 + 4 * (n))

static const uint8_t SYSTEM_HEADER[PS_SYSTEM] = { 0x00, 0x00, 0x01, 0xBB, 0x00, 0x0C, 0x80, 0xC3, 0x51, 0x80, 0xF0, 0x7F, 0xB9, 0xE0, 0x55, 0xBD, 0xE0, 0x08 };

void ps_group(PsWriter *w, const int *sizes, int n)
{
    w->table_n = 0;
    if (n < 1 || n > PS_TABLE_MAX)
        return;
    for (int k = 0; k < n; k++)
        if (sizes[k] < 1 || sizes[k] > 65535)
            return;
    int room_first = 2048 - PS_PACK - PS_SYSTEM - PS_TABLE_PACKET(n) - PS_PES_FIXED - PS_STAMPS;
    int room_other = 2048 - PS_PACK - PS_PES_FIXED - PS_STAMPS, room_next = 2048 - PS_PACK - PS_PES_FIXED;
    if (room_first < 1)
        return;
    int sector = 0;
    for (int k = 0; k < n; k++) {
        int first = k == 0 ? room_first : room_other;
        int sectors = 1 + (sizes[k] > first ? (sizes[k] - first + room_next - 1) / room_next : 0);
        sector += sectors; /* (every unit starts in a sector of its own) */
        w->table[k] = (uint16_t)sizes[k];
        if (k < 4)
            w->words[k] = (uint16_t)(sector - 1); /* the sector this unit ends in, counted from the first */
    }
    for (int k = n; k < 4; k++)
        w->words[k] = w->words[n - 1];
    w->table_n = n;
}

int ps_group_plan(PsWriter *w, int i, int count, int (*unit)(void *arg, int j, int *key), void *arg)
{
    int sizes[PS_TABLE_MAX], n = 0;
    for (int j = i; j < count && n < PS_TABLE_MAX; j++) {
        int key = 0, len = unit(arg, j, &key);
        if (len < 0)
            continue;
        if (n > 0 && key)
            break;
        sizes[n++] = len;
    }
    ps_group(w, sizes, n);
    return n;
}

static void put_ts(uint8_t *p, int prefix, int64_t ts)
{
    p[0] = (uint8_t)((prefix << 4) | (((ts >> 30) & 7) << 1) | 1);
    p[1] = (uint8_t)((ts >> 22) & 0xFF);
    p[2] = (uint8_t)((((ts >> 15) & 0x7F) << 1) | 1);
    p[3] = (uint8_t)((ts >> 7) & 0xFF);
    p[4] = (uint8_t)(((ts & 0x7F) << 1) | 1);
}

void ps_sector(PsWriter *w, uint8_t out[2048])
{
    memset(out, 0xFF, 2048);
    /* the pack header, with the system clock a little before the decoding time */
    int64_t scr = w->dts - 9000;
    if (scr < 0)
        scr = 0;
    out[0] = 0; out[1] = 0; out[2] = 1; out[3] = 0xBA;
    out[4] = (uint8_t)(0x44 | (((scr >> 30) & 7) << 3) | ((scr >> 28) & 3));
    out[5] = (uint8_t)((scr >> 20) & 0xFF);
    out[6] = (uint8_t)(0x04 | (((scr >> 15) & 0x1F) << 3) | ((scr >> 13) & 3));
    out[7] = (uint8_t)((scr >> 5) & 0xFF);
    out[8] = (uint8_t)(0x04 | ((scr & 0x1F) << 3));
    out[9] = 0x01;
    out[10] = 0x01; out[11] = 0x89; out[12] = 0xC3; /* 25200 * 50 bytes a second */
    out[13] = 0xF8;
    uint8_t *p = out + PS_PACK;
    int table = w->first && w->table_n > 0;
    if (table) { /* the start of a group: the system header and the table of the sizes of its units */
        memcpy(p, SYSTEM_HEADER, PS_SYSTEM);
        p += PS_SYSTEM;
        int n = w->table_n, len = 18 + 4 * n;
        p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xBF;
        p[4] = (uint8_t)(len >> 8); p[5] = (uint8_t)len;
        p[6] = 0x01; p[7] = 0xE0;
        for (int k = 0; k < 4; k++) {
            p[8 + 2 * k] = (uint8_t)(w->words[k] >> 8);
            p[9 + 2 * k] = (uint8_t)w->words[k];
        }
        memset(p + 16, 0, 4);
        p[20] = (uint8_t)((2 + 4 * n) >> 8); p[21] = (uint8_t)(2 + 4 * n);
        p[22] = (uint8_t)(n >> 8); p[23] = (uint8_t)n;
        for (int k = 0; k < n; k++) { /* (the flag is 0x0080, and 0 for the last unit of the table) */
            p[24 + 4 * k] = 0;
            p[25 + 4 * k] = k == n - 1 ? 0x00 : 0x80;
            p[26 + 4 * k] = (uint8_t)(w->table[k] >> 8);
            p[27 + 4 * k] = (uint8_t)w->table[k];
        }
        p += PS_TABLE_PACKET(n);
        w->table_n = 0;
    }
    int stamps = w->first ? PS_STAMPS : 0;
    int room = 2048 - (int)(p - out) - PS_PES_FIXED - stamps; /* payload that fits behind the PES header of this sector */
    int rest = w->len - w->pos;
    int payload = rest < room ? rest : room;
    int slack = room - payload;
    int stuff = 0, pad = 0;
    if (slack > 0 && slack < 6)
        stuff = slack; /* too little for a padding packet: stuffing bytes in the PES header */
    else
        pad = slack;
    int hdr = stamps + stuff;
    p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xE0;
    int pes_len = 3 + hdr + payload;
    p[4] = (uint8_t)(pes_len >> 8);
    p[5] = (uint8_t)(pes_len & 0xFF);
    p[6] = 0x81;
    p[7] = (uint8_t)(w->first ? 0xC1 : 0x00); /* time stamps (both) and the extension */
    p[8] = (uint8_t)hdr;
    uint8_t *q = p + 9;
    if (w->first) {
        put_ts(q, 3, w->pts);
        put_ts(q + 5, 1, w->dts);
        q[10] = 0x1E; /* the extension holds the size of the decoder's buffer (P-STD): 85 KB, as in the PSP's own streams */
        q[11] = 0x60;
        q[12] = 0x55;
    }
    q += stamps;
    for (int i = 0; i < stuff; i++)
        *q++ = 0xFF;
    memcpy(q, w->data + w->pos, (size_t)payload);
    q += payload;
    if (pad >= 6) { /* a padding packet to the end of the sector */
        q[0] = 0; q[1] = 0; q[2] = 1; q[3] = 0xBE;
        q[4] = (uint8_t)((pad - 6) >> 8);
        q[5] = (uint8_t)((pad - 6) & 0xFF);
        memset(q + 6, 0xFF, (size_t)(pad - 6));
    }
    w->pos += payload;
    w->first = 0;
}
