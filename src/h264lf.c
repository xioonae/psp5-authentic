/* psp5 authentic – the H.264 decoder in software: the loop filter (the standard's 8.7) and the borders of the reference pictures. */
#include <string.h>
#include <stdlib.h>

#include "h264int.h"

static const uint8_t ALPHA[52] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 5, 6, 7, 8, 9, 10, 12, 13, 15, 17, 20, 22, 25, 28, 32, 36, 40, 45, 50, 56, 63, 71, 80, 90, 101, 113, 127, 144, 162, 182, 203, 226, 255, 255 };
static const uint8_t BETA[52] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18 };
static const uint8_t TC0[52][3] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 1, 1}, {0, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 2}, {1, 1, 2}, {1, 1, 2}, {1, 1, 2}, {1, 2, 3}, {1, 2, 3}, {2, 2, 3},
    {2, 2, 4}, {2, 3, 4}, {2, 3, 4}, {3, 3, 5}, {3, 4, 6}, {3, 4, 6}, {4, 5, 7}, {4, 5, 8}, {4, 6, 9}, {5, 7, 10}, {6, 8, 11}, {6, 8, 13}, {7, 10, 14}, {8, 11, 16}, {9, 12, 18}, {10, 13, 20}, {11, 15, 23}, {13, 17, 25}
};

static inline int iabs(int v) { return v < 0 ? -v : v; }
static inline int clip3(int lo, int hi, int v) { return v < lo ? lo : v > hi ? hi : v; }
static inline uint8_t clip1(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* Four samples along an edge of luma (one segment, one strength); q is the first sample after the edge, xs the step across it, ys the one along it. The tests of the standard (|p0 - q0| < alpha ...)
 * are done as one unsigned comparison each. */
static void seg_luma(uint8_t *q, int xs, int ys, int bs, int alpha, int beta, int tc0)
{
    unsigned a2 = (unsigned)(2 * alpha - 1), b2 = (unsigned)(2 * beta - 1);
    for (int k = 0; k < 4; k++, q += ys) {
        int p0 = q[-xs], q0 = q[0];
        if ((unsigned)(p0 - q0 + alpha - 1) >= a2)
            continue;
        int p1 = q[-2 * xs], q1 = q[xs];
        if ((unsigned)(p1 - p0 + beta - 1) >= b2 || (unsigned)(q1 - q0 + beta - 1) >= b2)
            continue;
        int p2 = q[-3 * xs], q2 = q[2 * xs];
        int ap = iabs(p2 - p0), aq = iabs(q2 - q0);
        if (bs < 4) {
            int tc = tc0 + (ap < beta) + (aq < beta);
            int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
            if (ap < beta)
                q[-2 * xs] = (uint8_t)(p1 + clip3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1));
            if (aq < beta)
                q[xs] = (uint8_t)(q1 + clip3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1));
            q[-xs] = clip1(p0 + delta);
            q[0] = clip1(q0 - delta);
        } else {
            int small = iabs(p0 - q0) < ((alpha >> 2) + 2);
            if (ap < beta && small) {
                int p3 = q[-4 * xs];
                q[-xs] = (uint8_t)((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
                q[-2 * xs] = (uint8_t)((p2 + p1 + p0 + q0 + 2) >> 2);
                q[-3 * xs] = (uint8_t)((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
            } else {
                q[-xs] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
            }
            if (aq < beta && small) {
                int q3 = q[3 * xs];
                q[0] = (uint8_t)((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3);
                q[xs] = (uint8_t)((p0 + q0 + q1 + q2 + 2) >> 2);
                q[2 * xs] = (uint8_t)((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
            } else {
                q[0] = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
            }
        }
    }
}

static void edge_luma(uint8_t *q, int xs, int ys, const int8_t *bs, int alpha, int beta, int ia)
{
    for (int s = 0; s < 4; s++)
        if (bs[s] > 0)
            seg_luma(q + s * 4 * ys, xs, ys, bs[s], alpha, beta, bs[s] < 4 ? TC0[ia][bs[s] - 1] : 0);
}

/* two samples along an edge of chroma */
static void seg_chroma(uint8_t *q, int xs, int ys, int bs, int alpha, int beta, int tc)
{
    unsigned a2 = (unsigned)(2 * alpha - 1), b2 = (unsigned)(2 * beta - 1);
    for (int k = 0; k < 2; k++, q += ys) {
        int p0 = q[-xs], q0 = q[0];
        if ((unsigned)(p0 - q0 + alpha - 1) >= a2)
            continue;
        int p1 = q[-2 * xs], q1 = q[xs];
        if ((unsigned)(p1 - p0 + beta - 1) >= b2 || (unsigned)(q1 - q0 + beta - 1) >= b2)
            continue;
        if (bs < 4) {
            int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
            q[-xs] = clip1(p0 + delta);
            q[0] = clip1(q0 - delta);
        } else {
            q[-xs] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
            q[0] = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
        }
    }
}

/* 8 chroma samples along an edge; bs has the four strengths of the luma edge (two samples each) */
static void edge_chroma(uint8_t *q, int xs, int ys, const int8_t *bs, int alpha, int beta, int ia)
{
    for (int s = 0; s < 4; s++)
        if (bs[s] > 0)
            seg_chroma(q + s * 2 * ys, xs, ys, bs[s], alpha, beta, bs[s] < 4 ? TC0[ia][bs[s] - 1] + 1 : 0);
}

/* 0 or 1 by the motion of block pb of macroblock pm and block qb of macroblock qm (both inter, neither has coefficients): different pictures, or vectors a whole pixel apart */
static inline int strength_motion(const H264Dec *d, int pm, int pb, int qm, int qb)
{
    int rp = d->refpic[pm * 4 + ((pb >> 3) << 1) + ((pb & 3) >> 1)], rq = d->refpic[qm * 4 + ((qb >> 3) << 1) + ((qb & 3) >> 1)];
    if (rp != rq)
        return 1;
    const int16_t *mp = d->mv + (pm * 16 + pb) * 2, *mq = d->mv + (qm * 16 + qb) * 2;
    return iabs(mp[0] - mq[0]) >= 4 || iabs(mp[1] - mq[1]) >= 4;
}

void h264_deblock_picture(H264Dec *d)
{
    const H264Pps *pps = d->ap;
    int mbw = d->mbw;
    uint8_t *Y = d->cur->p[0], *U = d->cur->p[1], *V = d->cur->p[2];
    int sy = d->stride[0], sc = d->stride[1];
    for (int mby = 0; mby < d->mbh; mby++) {
        for (int mbx = 0; mbx < mbw; mbx++) {
            int mb = mby * mbw + mbx;
            if (d->slice_of[mb] < 0)
                continue;
            int idc = d->dbk_idc[mb];
            if (idc == 1)
                continue;
            int intra = d->kind[mb] != KIND_INTER;
            int left = mbx > 0 && d->slice_of[mb - 1] >= 0 && (idc != 2 || d->slice_of[mb - 1] == d->slice_of[mb]);
            int top = mby > 0 && d->slice_of[mb - mbw] >= 0 && (idc != 2 || d->slice_of[mb - mbw] == d->slice_of[mb]);
            int8_t bsv[4][4], bsh[4][4];
            int any = 0;
            if (intra) { /* intra: 3 inside, 4 on the edges of the macroblock */
                for (int e = 0; e < 4; e++)
                    for (int s = 0; s < 4; s++) {
                        bsv[e][s] = (int8_t)(e ? 3 : left ? 4 : 0);
                        bsh[e][s] = (int8_t)(e ? 3 : top ? 4 : 0);
                    }
                any = 1;
            } else {
                unsigned nzm = d->nzmask[mb];
                int uni = d->uni[mb]; /* the whole macroblock has one motion: no edge inside has a difference of it */
                for (int e = 0; e < 4; e++) {
                    unsigned vm, hm;
                    if (e) {
                        vm = ((nzm >> e) | (nzm >> (e - 1))) & 0x1111; /* bit 4 * s: the block or the one next to it has coefficients */
                        hm = ((nzm >> (4 * e)) | (nzm >> (4 * (e - 1)))) & 0xF;
                    } else {
                        vm = nzm & 0x1111;
                        hm = nzm & 0xF;
                    }
                    for (int s = 0; s < 4; s++) {
                        int v, h;
                        if (e == 0) {
                            if (!left)
                                v = 0;
                            else if (d->kind[mb - 1] != KIND_INTER)
                                v = 4;
                            else if ((vm >> (4 * s)) & 1 || (d->nzmask[mb - 1] >> (s * 4 + 3)) & 1)
                                v = 2;
                            else
                                v = strength_motion(d, mb - 1, s * 4 + 3, mb, s * 4);
                            if (!top)
                                h = 0;
                            else if (d->kind[mb - mbw] != KIND_INTER)
                                h = 4;
                            else if ((hm >> s) & 1 || (d->nzmask[mb - mbw] >> (12 + s)) & 1)
                                h = 2;
                            else
                                h = strength_motion(d, mb - mbw, 12 + s, mb, s);
                        } else {
                            v = (vm >> (4 * s)) & 1 ? 2 : uni ? 0 : strength_motion(d, mb, s * 4 + e - 1, mb, s * 4 + e);
                            h = (hm >> s) & 1 ? 2 : uni ? 0 : strength_motion(d, mb, (e - 1) * 4 + s, mb, e * 4 + s);
                        }
                        bsv[e][s] = (int8_t)v;
                        bsh[e][s] = (int8_t)h;
                        any |= v | h;
                    }
                }
            }
            if (!any)
                continue;
            int qpq = d->qp[mb];
            int a_off = d->dbk_a[mb] * 2, b_off = d->dbk_b[mb] * 2;
            uint8_t *y = Y + (ptrdiff_t)mby * 16 * sy + mbx * 16;
            uint8_t *u = U + (ptrdiff_t)mby * 8 * sc + mbx * 8, *v = V + (ptrdiff_t)mby * 8 * sc + mbx * 8;
            /* vertical edges, then horizontal ones; luma, then chroma */
            for (int dir = 0; dir < 2; dir++) {
                for (int e = 0; e < 4; e++) {
                    const int8_t *bs = dir ? bsh[e] : bsv[e];
                    if (!(bs[0] | bs[1] | bs[2] | bs[3]))
                        continue;
                    int nb = dir ? mb - mbw : mb - 1;
                    int qpp = e == 0 ? d->qp[nb] : qpq;
                    int qav = (qpp + qpq + 1) >> 1;
                    int ia = clip3(0, 51, qav + a_off), ib = clip3(0, 51, qav + b_off);
                    int alpha = ALPHA[ia], beta = BETA[ib];
                    if (!alpha || !beta)
                        continue;
                    uint8_t *q = y + (dir ? (ptrdiff_t)e * 4 * sy : e * 4);
                    edge_luma(q, dir ? sy : 1, dir ? 1 : sy, bs, alpha, beta, ia);
                    if (e == 0 || e == 2) {
                        int ce = e >> 1; /* chroma has two edges, at 0 and 4 */
                        int cqp_q = h264_chroma_qp[clip3(0, 51, qpq + pps->cqp_off)];
                        int cqp_p = e == 0 ? h264_chroma_qp[clip3(0, 51, qpp + pps->cqp_off)] : cqp_q;
                        int cav = (cqp_p + cqp_q + 1) >> 1;
                        int cia = clip3(0, 51, cav + a_off), cib = clip3(0, 51, cav + b_off);
                        int ca = ALPHA[cia], cb = BETA[cib];
                        if (ca && cb) {
                            ptrdiff_t off = dir ? (ptrdiff_t)ce * 4 * sc : ce * 4;
                            edge_chroma(u + off, dir ? sc : 1, dir ? 1 : sc, bs, ca, cb, cia);
                            edge_chroma(v + off, dir ? sc : 1, dir ? 1 : sc, bs, ca, cb, cia);
                        }
                    }
                }
            }
        }
    }
}

/* the borders of a finished picture: its edge pixels copied outwards, so that the motion compensation can read beyond the picture */
void h264_extend_borders(H264Dec *d, H264Pic *p)
{
    for (int pl = 0; pl < 3; pl++) {
        int w = d->mbw * (pl ? 8 : 16), h = d->mbh * (pl ? 8 : 16), pad = pl ? PAD_C : PAD_Y, st = d->stride[pl];
        uint8_t *base = p->p[pl];
        for (int y = 0; y < h; y++) {
            uint8_t *row = base + (ptrdiff_t)y * st;
            memset(row - pad, row[0], (size_t)pad);
            memset(row + w, row[w - 1], (size_t)pad);
        }
        for (int i = 1; i <= pad; i++) {
            memcpy(base - (ptrdiff_t)i * st - pad, base - pad, (size_t)(w + 2 * pad));
            memcpy(base + (ptrdiff_t)(h - 1 + i) * st - pad, base + (ptrdiff_t)(h - 1) * st - pad, (size_t)(w + 2 * pad));
        }
    }
}
