#!/usr/bin/env python3
"""
img2sprite.py - turn any picture into a C64 multicolour sprite grid.

A multicolour sprite is 12 by 21 fat pixels and every one of them is one
of four things: transparent, the colour in $D025, the colour in $D026, or
the sprite's own colour in $D027+n. Two of those three are shared by all
eight sprites, so a game fixes them once and each sprite only gets to
choose the third. That is the whole budget - 63 bytes - and this script
does nothing more than spend it as well as the source image allows.

The output is the ASCII block that assetgen.py already eats:

    ' '  transparent      '.'  bit pair 01 -> $D025
    '#'  bit pair 10 -> the sprite's own colour
    '+'  bit pair 11 -> $D026

Expect a silhouette, not a drawing. At this size the picture survives as
a shape and a couple of accents; everything else is decided by hand
afterwards. Treat what comes out as a starting point.

    ./img2sprite.py knight.png --name ART_STAND --preview /tmp/k.png
    ./img2sprite.py zombie.jpg --bg-corner --col 5 --flip

Options that matter:
    --mc0/--mc1   the two shared registers, as C64 colour numbers
                  (defaults 2 and 1, what GHOST KEEP uses)
    --col         the per-sprite colour; omit to let the fit choose it
    --pick-shared let the fit choose all three (tells you what shared
                  registers this image would want, if you are free to
                  set them)
"""

import argparse
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("needs Pillow:  pip install pillow")

# Pepto's PAL palette, the one everybody's screenshots look like.
C64 = [
    (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x68, 0x37, 0x2B),
    (0x70, 0xA4, 0xB2), (0x6F, 0x3D, 0x86), (0x58, 0x8D, 0x43),
    (0x35, 0x28, 0x79), (0xB8, 0xC7, 0x6F), (0x6F, 0x4F, 0x25),
    (0x43, 0x39, 0x00), (0x9A, 0x67, 0x59), (0x44, 0x44, 0x44),
    (0x6C, 0x6C, 0x6C), (0x9A, 0xD2, 0x84), (0x6C, 0x5E, 0xB5),
    (0x95, 0x95, 0x95),
]
C64_NAMES = [
    "black", "white", "red", "cyan", "purple", "green", "blue", "yellow",
    "orange", "brown", "lt red", "dk grey", "md grey", "lt green",
    "lt blue", "lt grey",
]

# bit pair -> the character assetgen.py expects (MC_SPR, inverted)
CHARS = {0: " ", 1: ".", 2: "#", 3: "+"}


def dist(a, b):
    """Squared distance with the usual eyeball weights: green counts most,
    blue least. Good enough, and Lab would not change the pick at 12x21."""
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return 2 * dr * dr + 4 * dg * dg + db * db


def load_mask(im, args):
    """RGB image plus a boolean 'this pixel is part of the subject' mask."""
    im = im.convert("RGBA")
    px = im.load()
    w, h = im.size
    opaque = [[False] * w for _ in range(h)]

    has_alpha = any(px[x, y][3] < 250
                    for y in range(0, h, max(1, h // 40))
                    for x in range(0, w, max(1, w // 40)))

    bg = None
    if args.bg is not None:
        bg = tuple(int(args.bg[i:i + 2], 16) for i in (0, 2, 4))
    elif not has_alpha or args.bg_corner:
        corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
        bg = tuple(sum(c[i] for c in corners) // 4 for i in range(3))

    tol = args.bg_tol * args.bg_tol * 7      # same weights as dist()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < args.alpha:
                continue
            if bg is not None and dist((r, g, b), bg) <= tol:
                continue
            opaque[y][x] = True
    return im.convert("RGB"), opaque


def crop_to_subject(im, opaque, pad):
    w, h = im.size
    xs = [x for y in range(h) for x in range(w) if opaque[y][x]]
    ys = [y for y in range(h) for x in range(w) if opaque[y][x]]
    if not xs:
        sys.exit("everything was read as background - try --bg-tol or --bg")
    x0, x1 = max(0, min(xs) - pad), min(w, max(xs) + 1 + pad)
    y0, y1 = max(0, min(ys) - pad), min(h, max(ys) + 1 + pad)
    sub = [[opaque[y][x] for x in range(x0, x1)] for y in range(y0, y1)]
    return im.crop((x0, y0, x1, y1)), sub


def to_grid(im, opaque, cols, rows, stretch=False):
    """Average the source down to cols x rows cells, keeping the aspect the
    C64 will show: a sprite pixel is twice as wide as it is tall, so the
    cell grid is really cols*2 by rows on screen.

    With stretch the aspect is thrown away and the subject fills the whole
    sprite. That is usually what you want: a standing figure is far taller
    than 24 by 21, and honouring its aspect spends half the sprite on
    empty air. Squashing a person into the box is exactly what the artists
    of the day did."""
    w, h = im.size
    if stretch:
        nw, nh = w, h
    else:
        target = (cols * 2) / rows
        if w / h > target:                   # too wide: pad top and bottom
            nh, nw = int(round(w / target)), w
        else:                                # too tall: pad left and right
            nw, nh = int(round(h * target)), h
    ox, oy = (nw - w) // 2, (nh - h) // 2

    canvas = Image.new("RGB", (nw, nh), (0, 0, 0))
    canvas.paste(im, (ox, oy))
    mask = Image.new("L", (nw, nh), 0)
    mpx = mask.load()
    for y in range(h):
        row = opaque[y]
        for x in range(w):
            if row[x]:
                mpx[ox + x, oy + y] = 255

    small = canvas.resize((cols, rows), Image.BOX)
    smask = mask.resize((cols, rows), Image.BOX)
    return small.load(), smask.load()


def fit(pixels, cover, cols, rows, palette, coverage):
    """Nearest of the three ink colours for every covered cell."""
    grid, err = [], 0
    for y in range(rows):
        line = []
        for x in range(cols):
            if cover[x, y] < coverage:
                line.append(None)          # not black: nothing at all
                continue
            c = pixels[x, y]
            best, bestd = palette[0], None
            for idx in palette:
                d = dist(c, C64[idx])
                if bestd is None or d < bestd:
                    best, bestd = idx, d
            err += bestd
            line.append(best)
        grid.append(line)
    return grid, err


def luma(idx):
    r, g, b = C64[idx]
    return 2 * r + 5 * g + b


def add_outline(grid, inks):
    """Ring the shape with the darkest ink, leaving what is inside alone.
    A rim is what tells the eye where the arm ends and the sky begins, and
    a fitted sprite has none: the source image had a background doing that
    job. Skipped when the shape is too thin to have an inside, because
    there the outline would eat the whole sprite."""
    rows, cols = len(grid), len(grid[0])
    dark = min(inks, key=luma)

    def covered(x, y):
        return 0 <= x < cols and 0 <= y < rows and grid[y][x] is not None

    rim = [[covered(x, y) and not (covered(x - 1, y) and covered(x + 1, y)
                                   and covered(x, y - 1) and covered(x, y + 1))
            for x in range(cols)] for y in range(rows)]

    inked = sum(1 for y in range(rows) for x in range(cols) if covered(x, y))
    inside = inked - sum(r.count(True) for r in rim)
    if inked == 0 or inside * 4 < inked:
        return grid

    out = []
    for y in range(rows):
        line = []
        for x in range(cols):
            c = grid[y][x]
            if c is None:
                line.append(None)
            elif rim[y][x]:
                line.append(dark)
            else:
                # an inside cell the same colour as the rim would erase the
                # rim; push it to the next ink up
                line.append(c if c != dark
                            else max(inks, key=luma))
            line[-1] = line[-1]
        out.append(line)
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("image")
    p.add_argument("--name", default="SPRITE")
    p.add_argument("--cols", type=int, default=12)
    p.add_argument("--rows", type=int, default=21)
    p.add_argument("--mc0", type=int, default=2, help="$D025, shared")
    p.add_argument("--mc1", type=int, default=1, help="$D026, shared")
    p.add_argument("--col", type=int, default=None, help="per-sprite colour")
    p.add_argument("--pick-shared", action="store_true",
                   help="choose all three colours, not just the sprite's")
    p.add_argument("--bg", help="background colour to drop, as RRGGBB")
    p.add_argument("--bg-corner", action="store_true",
                   help="take the background colour from the corners")
    p.add_argument("--bg-tol", type=int, default=48)
    p.add_argument("--alpha", type=int, default=128)
    p.add_argument("--pad", type=int, default=0, help="border kept on crop")
    p.add_argument("--no-crop", action="store_true",
                   help="keep the picture's own framing instead of zooming "
                        "to the subject")
    p.add_argument("--coverage", type=int, default=96,
                   help="how much of a cell must be subject (0-255)")
    p.add_argument("--stretch", action="store_true",
                   help="fill the sprite, ignoring the subject's aspect")
    p.add_argument("--ink", type=int, default=None,
                   help="take only the shape and paint it all in this "
                        "colour. What a flat silhouette actually has to "
                        "give: fitting its colours faithfully yields a "
                        "black sprite on a black sky")
    p.add_argument("--outline", action="store_true",
                   help="darkest ink around the rim: a flat silhouette has "
                        "no shading of its own and reads as a blob without "
                        "one")
    p.add_argument("--flip", action="store_true", help="mirror horizontally")
    p.add_argument("--preview", help="write a PNG of the result")
    p.add_argument("--scale", type=int, default=12)
    args = p.parse_args()

    im = Image.open(args.image)
    im, opaque = load_mask(im, args)
    if not args.no_crop:
        im, opaque = crop_to_subject(im, opaque, args.pad)
    if args.flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
        opaque = [list(reversed(r)) for r in opaque]
    pixels, cover = to_grid(im, opaque, args.cols, args.rows, args.stretch)

    # Which three inks? Either the caller fixed them, or we try every
    # candidate and keep the cheapest fit. 560 combinations is nothing.
    if args.ink is not None:
        combos = [(args.mc0, args.mc1, args.ink)]
    elif args.pick_shared:
        combos = [(a, b, c)
                  for a in range(16) for b in range(a + 1, 16)
                  for c in range(b + 1, 16)]
    elif args.col is None:
        combos = [(args.mc0, args.mc1, c) for c in range(16)
                  if c not in (args.mc0, args.mc1)]
    else:
        combos = [(args.mc0, args.mc1, args.col)]

    best = None
    for combo in combos:
        grid, err = fit(pixels, cover, args.cols, args.rows,
                        list(combo), args.coverage)
        if best is None or err < best[1]:
            best = (grid, err, combo)
    grid, _, combo = best

    # combo is (mc0, mc1, own) in the fixed case; when the fit was free to
    # pick all three, the assignment to registers is ours to make - put the
    # most used colour on the sprite's own register, since that is the one
    # every sprite gets to set for itself.
    if args.pick_shared:
        counts = {c: sum(r.count(c) for r in grid) for c in combo}
        own = max(counts, key=counts.get)
        rest = [c for c in combo if c != own]
        mc0, mc1 = rest[0], rest[1]
    else:
        mc0, mc1, own = combo

    if args.ink is not None:                 # shape only, one flat colour
        grid = [[None if c is None else args.ink for c in row]
                for row in grid]
    if args.outline:
        grid = add_outline(grid, [mc0, mc1, own])

    pair = {own: 2, mc0: 1, mc1: 3}
    rows_txt = ["".join(" " if c is None else CHARS[pair[c]] for c in row)
                for row in grid]

    print(f'    ("{args.name}", [')
    for r in rows_txt:
        print(f'        "{r}",')
    print("    ]),")
    print(f"\n# $D025 = {mc0} ({C64_NAMES[mc0]}), "
          f"$D026 = {mc1} ({C64_NAMES[mc1]}), "
          f"sprite colour = {own} ({C64_NAMES[own]})", file=sys.stderr)
    filled = sum(1 for r in rows_txt for ch in r if ch != " ")
    print(f"# {filled} of {args.cols * args.rows} cells inked",
          file=sys.stderr)

    if args.preview:
        s = args.scale
        out = Image.new("RGB", (args.cols * 2 * s, args.rows * s),
                        (30, 30, 30))
        d = out.load()
        for y, row in enumerate(grid):
            for x, c in enumerate(row):
                if c is None:
                    continue
                rgb = C64[c]
                for yy in range(y * s, y * s + s):
                    for xx in range(x * 2 * s, x * 2 * s + 2 * s):
                        d[xx, yy] = rgb
        out.save(args.preview)
        print(f"# preview: {args.preview}", file=sys.stderr)


if __name__ == "__main__":
    main()
