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
    w->dts = dts < 0 ? 0 : dts;
    w->first = 1;
}

int ps_pending(const PsWriter *w)
{
    return w->len - w->pos;
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
    int stamps = w->first ? (w->dts != w->pts ? 10 : 5) : 0;
    int room = 2048 - 14 - 9 - stamps; /* payload that fits behind the PES header of this sector */
    int rest = w->len - w->pos;
    int payload = rest < room ? rest : room;
    int slack = room - payload;
    int stuff = 0, pad = 0;
    if (slack > 0 && slack < 6)
        stuff = slack; /* too little for a padding packet: stuffing bytes in the PES header */
    else
        pad = slack;
    int hdr = stamps + stuff;
    uint8_t *p = out + 14;
    p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xE0;
    int pes_len = 3 + hdr + payload;
    p[4] = (uint8_t)(pes_len >> 8);
    p[5] = (uint8_t)(pes_len & 0xFF);
    p[6] = 0x80;
    p[7] = (uint8_t)(w->first ? (stamps == 10 ? 0xC0 : 0x80) : 0x00);
    p[8] = (uint8_t)hdr;
    uint8_t *q = p + 9;
    if (stamps == 10) {
        put_ts(q, 3, w->pts);
        put_ts(q + 5, 1, w->dts);
    } else if (stamps == 5) {
        put_ts(q, 2, w->pts);
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
