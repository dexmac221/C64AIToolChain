#!/usr/bin/env python3
"""band_edges.py DIR - do the sky's colour changes show inside the window?

For every band edge in the VICE screenshots in DIR (384x272: the
38-column window is pixels 39..342, the border starts at 343), find the
edge in the middle of the line (16 pixels of colour a on two lines, b on
the next two) and judge each end of the window separately, when that end
is open sky: the last 16 visible pixels of the row's last line must not
show the new colour (a write too early), the first 16 of the next row's
first line must not show the old one (a write too late).
Result on evidence/band_edges/ (32 shots, all eight phases): 40 right
ends and 160 left ends checked, 0 early, 0 late.
"""
import sys, glob
from PIL import Image
L0, R1, BORDER = 39, 342, 343
ok = early = lok = late = 0
for f in sorted(glob.glob(sys.argv[1] + "/e*.png")):
    px = Image.open(f).convert("RGB").load()
    assert px[BORDER, 120] == (0, 0, 0)
    for y in range(73, 245):
        # a sky band edge between y and y+1: a stretch of 16 pixels in the middle
        # with colour a on lines y-1, y and b on y+1, y+2
        for x0 in range(60, 320, 16):
            col = lambda yy: {px[x, yy] for x in range(x0, x0 + 16)}
            A1, A2, B1, B2 = col(y - 1), col(y), col(y + 1), col(y + 2)
            if len(A1) == len(A2) == len(B1) == len(B2) == 1 and A1 == A2 and B1 == B2 and A1 != B1:
                a, b = next(iter(A1)), next(iter(B1)); break
        else:
            continue
        if px[R1, y - 1] == a and px[R1, y + 2] == b:          # the right end is sky
            tail = [px[x, y] for x in range(R1 - 15, R1 + 1)]
            if b in tail: early += 1; print("EARLY", f[-12:], y, tail)
            else: ok += 1
        if px[L0, y - 1] == a and px[L0, y + 2] == b:          # the left end is sky
            head = [px[x, y + 1] for x in range(L0, L0 + 16)]
            if a in head: late += 1; print("LATE", f[-12:], y + 1, head)
            else: lok += 1
print("band edges, right end of the row's last line: %d clean, %d showing the new colour early" % (ok, early))
print("band edges, left end of the next row's first line: %d clean, %d showing the old colour late" % (lok, late))
