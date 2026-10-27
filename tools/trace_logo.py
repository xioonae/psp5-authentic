#!/usr/bin/env python3
# psp5 – made by Mille Weber
# Makes the outline of the boot logo (logo_len / logo_pts in src/icons.c) from a reference picture of it: a white shape on a transparent background.
# The edge is found where the picture is half transparent (marching squares on the alpha channel), every closed outline is then simplified until no
# corner is more than EPS pixels of the picture off the real edge, and the corners are printed in half pixels of the picture, as icons.c wants them.
# The picture is only needed to make this table; nothing in psp5 reads it.
#
#   python3 tools/trace_logo.py picture.png [EPS]      (EPS defaults to 1.0; needs numpy and Pillow)
import sys, math
import numpy as np
from PIL import Image

PAD = 2  # a margin of empty pixels, so that a logo that touches the edge of the picture still gives closed outlines


def trace(alpha):
    a = np.pad(alpha, PAD)
    inside = a >= 0.5
    tl, tr, br, bl = inside[:-1, :-1], inside[:-1, 1:], inside[1:, 1:], inside[1:, :-1]
    case = tl * 8 + tr * 4 + br * 2 + bl * 1
    top, bottom = lambda y, x: ('h', y, x), lambda y, x: ('h', y + 1, x)
    left, right = lambda y, x: ('v', y, x), lambda y, x: ('v', y, x + 1)
    segs = {1: [(left, bottom)], 2: [(bottom, right)], 3: [(left, right)], 4: [(top, right)], 5: [(left, top), (bottom, right)], 6: [(top, bottom)],
            7: [(left, top)], 8: [(left, top)], 9: [(top, bottom)], 10: [(left, bottom), (top, right)], 11: [(top, right)], 12: [(left, right)],
            13: [(bottom, right)], 14: [(left, bottom)]}
    adj = {}
    for y, x in zip(*np.nonzero((case != 0) & (case != 15))):
        for f, g in segs[int(case[y, x])]:
            e1, e2 = f(y, x), g(y, x)
            adj.setdefault(e1, []).append(e2)
            adj.setdefault(e2, []).append(e1)

    def point(e):  # where the edge crosses between two pixels, in picture pixels
        k, y, x = e
        v0, v1 = (a[y, x], a[y, x + 1]) if k == 'h' else (a[y, x], a[y + 1, x])
        f = (0.5 - v0) / (v1 - v0) if v1 != v0 else 0.5
        return (x + f + 0.5 - PAD, y + 0.5 - PAD) if k == 'h' else (x + 0.5 - PAD, y + f + 0.5 - PAD)

    loops, seen = [], set()
    for start in list(adj):
        if start in seen:
            continue
        loop, prev, cur = [start], None, start
        seen.add(start)
        while True:
            nxt = [n for n in adj[cur] if n != prev]
            nxt = [n for n in nxt if n not in seen] or [n for n in nxt if n == start]
            if not nxt or nxt[0] == start:
                break
            loop.append(nxt[0])
            seen.add(nxt[0])
            prev, cur = cur, nxt[0]
        loops.append([point(e) for e in loop])
    return [l for l in loops if abs(area(l)) > 2000]  # (specks are not part of the logo)


def area(p):
    return 0.5 * sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))


def contains(poly, p):
    c = False
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % len(poly)]
        if (y1 > p[1]) != (y2 > p[1]) and p[0] < (x2 - x1) * (p[1] - y1) / (y2 - y1) + x1:
            c = not c
    return c


def simplify(pts, eps):
    # Douglas-Peucker on a closed loop: split it at the point farthest from the first, simplify the two halves
    def dp(p, i, j, keep):
        if j <= i + 1:
            return
        (x1, y1), (x2, y2) = p[i], p[j]
        dx, dy = x2 - x1, y2 - y1
        ln = math.hypot(dx, dy)
        best, bi = -1, -1
        for k in range(i + 1, j):
            d = abs(dy * (p[k][0] - x1) - dx * (p[k][1] - y1)) / ln if ln else math.hypot(p[k][0] - x1, p[k][1] - y1)
            if d > best:
                best, bi = d, k
        if best > eps:
            keep[bi] = True
            dp(p, i, bi, keep)
            dp(p, bi, j, keep)

    n = len(pts)
    i1 = max(range(n), key=lambda k: math.hypot(pts[k][0] - pts[0][0], pts[k][1] - pts[0][1]))
    halves = [pts[:i1 + 1], pts[i1:] + [pts[0]]]
    out = []
    for h in halves:
        keep = [False] * len(h)
        keep[0] = keep[-1] = True
        dp(h, 0, len(h) - 1, keep)
        out += [p for p, k in zip(h, keep) if k][:-1]
    return out


def main():
    img = Image.open(sys.argv[1]).convert('RGBA')
    eps = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    loops = trace(np.array(img)[..., 3].astype(np.float32) / 255)
    # every shape goes round the same way (a shape inside another, a hole, would have to go the other way round: the logo has none)
    for i, l in enumerate(loops):
        if any(j != i and contains(o, l[0]) for j, o in enumerate(loops)):
            sys.exit('this picture has a hole in a shape; icons.c draws solid shapes only')
    loops = [l if area(l) > 0 else l[::-1] for l in loops]
    shapes = []
    for l in loops:
        pts = []
        for x, y in simplify(l, eps):
            q = [round(x * 2), round(y * 2)]
            if not pts or pts[-1] != q:
                pts.append(q)
        if pts[0] == pts[-1]:
            pts.pop()
        shapes.append(pts)
    print('#define LOGO_UNITS_W %d' % (img.width * 2))
    print('#define LOGO_UNITS_H %d' % (img.height * 2))
    print('#define LOGO_SHAPES %d' % len(shapes))
    print('static const unsigned char logo_len[LOGO_SHAPES] = { %s };' % ', '.join(str(len(s)) for s in shapes))
    print('static const unsigned short logo_pts[] = {')
    flat = [p for s in shapes for p in s]
    for i in range(0, len(flat), 6):
        print('    ' + ' '.join('%d, %d,' % (x, y) for x, y in flat[i:i + 6]))
    print('};')


main()
