/* psp5 – made by Mille Weber */
#include "lzo.h"

/* LZO1X decompression, for .jso files (libcisoread calls lzo1x_decompress for every block of a JSO that is packed with LZO).
 * Written from the LZO1X stream format; checked against the reference LZO library (host-tests/units/lzo_test.c). It never
 * reads past the end of the input and never writes past *dst_len bytes, whatever the data says. */

#define NEED_IP(n) do { if ((size_t)(ip_end - ip) < (size_t)(n)) goto input_overrun; } while (0)
#define NEED_OP(n) do { if ((size_t)(op_end - op) < (size_t)(n)) goto output_overrun; } while (0)

/* The length of a run is 0 in its first byte and then a number of zero bytes (255 each) and a last byte that is not zero. */
#define RUN_EXTRA(base, t)                                                                                              \
    do {                                                                                                                \
        size_t zeros = 0;                                                                                               \
        NEED_IP(1);                                                                                                     \
        while (*ip == 0) {                                                                                              \
            ip++;                                                                                                       \
            zeros++;                                                                                                    \
            NEED_IP(1);                                                                                                 \
        }                                                                                                               \
        (t) += zeros * 255 + (base) + *ip++;                                                                            \
    } while (0)

int lzo1x_decompress(const void *src, unsigned src_len, void *dst, unsigned *dst_len, void *wrk)
{
    const unsigned char *ip = (const unsigned char *)src;
    const unsigned char *const ip_end = ip + src_len;
    unsigned char *const out = (unsigned char *)dst;
    unsigned char *op = out;
    unsigned char *const op_end = out + *dst_len;
    size_t t = 0, next = 0, dist;
    int state = 0; /* 0: start or after a match with no literals, 1..3: literals that followed a match, 4: after a literal run */
    int result;

    if (src_len < 3)
        goto input_overrun;
    if (*ip > 17) { /* a first literal run is coded in the first byte */
        t = *ip++ - 17;
        if (t < 4) {
            next = t;
            goto match_next;
        }
        goto copy_literals;
    }

    for (;;) {
        NEED_IP(1);
        t = *ip++;
        if (t < 16) {
            if (state == 0) { /* a run of literals */
                if (t == 0)
                    RUN_EXTRA(15, t);
                t += 3;
            copy_literals:
                NEED_IP(t);
                NEED_OP(t);
                do {
                    *op++ = *ip++;
                } while (--t);
                state = 4;
                continue;
            }
            /* a short match right after literals */
            NEED_IP(1);
            next = t & 3;
            dist = 1 + (t >> 2) + ((size_t)*ip++ << 2);
            if (state == 4) {
                dist += 0x0800; /* after a run of literals the same code reaches 2 KB further back, and is 3 bytes long */
                t = 3;
            } else {
                t = 2;
            }
        } else if (t >= 64) { /* M2: up to 2 KB back, 3 to 8 bytes */
            NEED_IP(1);
            next = t & 3;
            dist = 1 + ((t >> 2) & 7) + ((size_t)*ip++ << 3);
            t = (t >> 5) + 1;
        } else if (t >= 32) { /* M3: up to 16 KB back */
            t = (t & 31) + 2;
            if (t == 2)
                RUN_EXTRA(31, t);
            NEED_IP(2);
            next = ip[0] | ((size_t)ip[1] << 8);
            ip += 2;
            dist = 1 + (next >> 2);
            next &= 3;
        } else { /* M4: 16 KB to 48 KB back, or the end of the data */
            dist = (t & 8) << 11;
            t = (t & 7) + 2;
            if (t == 2)
                RUN_EXTRA(7, t);
            NEED_IP(2);
            next = ip[0] | ((size_t)ip[1] << 8);
            ip += 2;
            dist += next >> 2;
            next &= 3;
            if (dist == 0) { /* the end marker (17, 0, 0) */
                result = t == 3 ? (ip == ip_end ? 0 : ip < ip_end ? -8 : -4) : -1;
                *dst_len = (unsigned)(op - out);
                return result;
            }
            dist += 0x4000;
        }
        if (dist > (size_t)(op - out)) { /* before the start of the output */
            result = -6;
            goto done;
        }
        NEED_OP(t);
        {
            const unsigned char *m = op - dist;
            do {
                *op++ = *m++; /* one byte at a time: the match may run into the bytes it is writing */
            } while (--t);
        }
    match_next:
        state = (int)next;
        if (next) { /* 0..3 literals follow the match */
            NEED_IP(next);
            NEED_OP(next);
            t = next;
            do {
                *op++ = *ip++;
            } while (--t);
        }
    }

input_overrun:
    result = -4;
    goto done;
output_overrun:
    result = -5;
done:
    *dst_len = (unsigned)(op - out);
    return result;
}
