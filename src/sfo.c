/* psp5 – made by Mille Weber */
#include <stdio.h>
#include <string.h>
#include "sfo.h"

#define SFO_MAGIC 0x46535000 /* "\0PSF" */

typedef struct __attribute__((packed)) {
    u32 magic;
    u32 version;
    u32 key_table;
    u32 data_table;
    u32 count;
} SfoHeader;

typedef struct __attribute__((packed)) {
    u16 key_offset;
    u16 fmt;
    u32 len;
    u32 max_len;
    u32 data_offset;
} SfoEntry;

/* SFO-huvudet och posttabellen måste rymmas i datat (u64: ett trasigt antal får inte svämma över) */
static int sfo_valid(const u8 *sfo, u32 sfo_size)
{
    const SfoHeader *h = (const SfoHeader *)sfo;
    if (sfo_size < sizeof(SfoHeader) || h->magic != SFO_MAGIC)
        return 0;
    return (u64)sizeof(SfoHeader) + (u64)h->count * sizeof(SfoEntry) <= sfo_size;
}

/* A PARAM.SFO comes from any file on the card (an ISO, an EBOOT.PBP, a folder), so nothing in it is trusted: every offset is added in 64 bits (a key table or data offset near 2^32 must not wrap
 * round to a place inside the buffer), a key has to end inside the data (strcmp would run on past the end), and an output of no room is not written at all. */
int sfo_get_string(const u8 *sfo, u32 sfo_size, const char *key, char *out, int out_size)
{
    if (!sfo || !key || !out || out_size < 1 || !sfo_valid(sfo, sfo_size))
        return 0;
    const SfoHeader *h = (const SfoHeader *)sfo;
    const SfoEntry *e = (const SfoEntry *)(sfo + sizeof(SfoHeader));
    size_t klen = strlen(key);
    for (u32 i = 0; i < h->count; i++) {
        u64 koff = (u64)h->key_table + e[i].key_offset;
        if (koff >= sfo_size || klen >= sfo_size - koff || memcmp(sfo + koff, key, klen) != 0 || sfo[koff + klen] != 0)
            continue; /* (not this key, or a key that does not end inside the data) */

        u64 doff = (u64)h->data_table + e[i].data_offset;
        u32 len = e[i].len;
        if (doff + len > sfo_size)
            return 0;
        if (len >= (u32)out_size)
            len = (u32)out_size - 1;
        memcpy(out, sfo + doff, len);
        out[len] = 0;
        return 1;
    }
    return 0;
}

int sfo_count(const u8 *sfo, u32 sfo_size)
{
    return sfo_valid(sfo, sfo_size) ? (int)((const SfoHeader *)sfo)->count : 0;
}

int sfo_entry(const u8 *sfo, u32 sfo_size, int i, char *key, int key_size, char *val, int val_size)
{
    if (i < 0 || i >= sfo_count(sfo, sfo_size) || key_size < 1 || val_size < 1)
        return 0;
    const SfoHeader *h = (const SfoHeader *)sfo;
    const SfoEntry *e = (const SfoEntry *)(sfo + sizeof(SfoHeader)) + i;

    u64 koff = (u64)h->key_table + e->key_offset;
    if (koff >= sfo_size)
        return 0;
    u32 klen = 0;
    while (koff + klen < sfo_size && sfo[koff + klen] && klen < (u32)key_size - 1)
        klen++;
    if (koff + klen >= sfo_size)
        return 0; /* nyckeln fortsätter förbi slutet */
    memcpy(key, sfo + koff, klen);
    key[klen] = 0;

    u64 doff = (u64)h->data_table + e->data_offset;
    u32 len = e->len;
    if (doff + len > sfo_size)
        return 0;
    const u8 *d = sfo + doff;
    if (e->fmt == 0x0404 && len == 4) { /* 32-bitarstal */
        snprintf(val, val_size, "%u", (unsigned)(d[0] | (d[1] << 8) | (d[2] << 16) | ((u32)d[3] << 24)));
    } else if (e->fmt == 0x0004 || e->fmt == 0x0204) { /* text, med eller utan avslutande 0 */
        u32 n = 0;
        while (n < len && d[n] && n < (u32)val_size - 1)
            n++;
        memcpy(val, d, n);
        val[n] = 0;
    } else { /* annat: de första byten i hex */
        int k = 0;
        for (u32 b = 0; b < len && b < 8 && k + 3 < val_size; b++)
            k += snprintf(val + k, val_size - k, "%02X", d[b]);
        val[k] = 0;
    }
    return 1;
}
