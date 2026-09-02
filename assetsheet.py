#!/usr/bin/env python3
"""
assetsheet.py - draw a game's assetgen.py art big enough to judge.

The other direction from img2sprite.py, and the more useful one. Art in
this toolchain is authored as ASCII inside assetgen.py and then only ever
seen 21 pixels tall inside a 384x272 screenshot - which is to say, never
seen at all. This renders every sprite and tile at the colours the game
actually sets, magnified, labelled, on the game's own background, so the
thing that drew them can look at what it drew.

    ./assetsheet.py ghosts/assetgen.py --out /tmp/ghosts.png
    ./assetsheet.py ghosts/assetgen.py --only ART --scale 16
    ./assetsheet.py ghosts/assetgen.py --strip ART_RUN1,ART_RUN2,ART_STAND

Colours: sprites are told their per-sprite colour by name, since the game
assigns those at runtime and the ASCII does not know them. The two shared
sprite registers and the three character registers are flags. Defaults
are GHOST KEEP's.

Bit pairs differ between the two, which is the trap this toolchain has
already fallen into once:
    tiles    ' '=$D021  '.'=$D022  '+'=$D023  '#'=colour RAM
    sprites  ' '=none   '.'=$D025  '#'=own    '+'=$D026
"""

import argparse
import importlib.util
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("needs Pillow:  pip install pillow")

C64 = [
    (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x68, 0x37, 0x2B),
    (0x70, 0xA4, 0xB2), (0x6F, 0x3D, 0x86), (0x58, 0x8D, 0x43),
    (0x35, 0x28, 0x79), (0xB8, 0xC7, 0x6F), (0x6F, 0x4F, 0x25),
    (0x43, 0x39, 0x00), (0x9A, 0x67, 0x59), (0x44, 0x44, 0x44),
    (0x6C, 0x6C, 0x6C), (0x9A, 0xD2, 0x84), (0x6C, 0x5E, 0xB5),
    (0x95, 0x95, 0x95),
]

# GHOST KEEP's assignments; every one of them is a flag.
DEF_SPRITE_COL = [
    ("ART_", 14), ("LANCE", 7), ("ZOM_", 5), ("CROW", 11), ("PUFF", 1),
]


def load(path):
    spec = importlib.util.spec_from_file_location("assetgen", path)
    mod = importlib.util.module_from_spec(spec)
    sys.path.insert(0, path.rsplit("/", 1)[0] if "/" in path else ".")
    spec.loader.exec_module(mod)
    return mod


def font(size, bold=False):
    base = "/usr/share/fonts/truetype/dejavu/DejaVuSans"
    try:
        return ImageFont.truetype(f"{base}{'-Bold' if bold else ''}.ttf", size)
    except OSError:
        return ImageFont.load_default()


def sprite_colours(name, args):
    for prefix, col in args.sprite_col:
        if name.startswith(prefix):
            own = col
            break
    else:
        own = args.default_col
    return {".": C64[args.mc0], "#": C64[own], "+": C64[args.mc1]}, own


def draw_cell(img, x0, y0, rows, cmap, scale, wide, bg=None):
    d = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            c = cmap.get(ch, bg)
            if c is None:
                continue
            for yy in range(y0 + y * scale, y0 + (y + 1) * scale):
                for xx in range(x0 + x * scale * wide,
                                x0 + (x + 1) * scale * wide):
                    if 0 <= xx < img.width and 0 <= yy < img.height:
                        d[xx, yy] = c


def main():
    p = argparse.ArgumentParser()
    p.add_argument("assetgen")
    p.add_argument("--out", default="assetsheet.png")
    p.add_argument("--scale", type=int, default=9)
    p.add_argument("--cols", type=int, default=6, help="items per row")
    p.add_argument("--only", help="name prefix filter")
    p.add_argument("--strip", help="comma-separated names, drawn in a row "
                                   "shoulder to shoulder: how an animation "
                                   "reads is a property of the sequence, not "
                                   "of any one frame")
    p.add_argument("--tiles", action="store_true", help="tiles instead")
    p.add_argument("--mc0", type=int, default=2, help="sprites, $D025")
    p.add_argument("--mc1", type=int, default=1, help="sprites, $D026")
    p.add_argument("--default-col", type=int, default=5)
    p.add_argument("--bg", type=int, default=0, help="$D021")
    p.add_argument("--d022", type=int, default=11)
    p.add_argument("--d023", type=int, default=9)
    p.add_argument("--colram", type=int, default=5, help="tiles, '#'")
    p.add_argument("--grid", action="store_true",
                   help="rule every cell, to count pixels by eye")
    p.add_argument("--sprite-col", default="",
                   help="PREFIX=col,... per-sprite colours by name prefix "
                        "(default: GHOST KEEP's)")
    args = p.parse_args()
    if args.sprite_col:
        args.sprite_col = [(kv.split("=")[0], int(kv.split("=")[1]))
                           for kv in args.sprite_col.split(",")]
    else:
        args.sprite_col = DEF_SPRITE_COL

    mod = load(args.assetgen)
    items = mod.TILES if args.tiles else mod.SPRITES
    if args.only:
        items = [i for i in items if i[0].startswith(args.only)]
    if args.strip:
        want = args.strip.split(",")
        items = sorted([i for i in items if i[0] in want],
                       key=lambda i: want.index(i[0]))
        args.cols = len(items)
    if not items:
        sys.exit("nothing matched")

    s = args.scale
    wide = 2                              # every pixel is double width
    ch, cw = len(items[0][1]), len(items[0][1][0])
    iw, ih = cw * s * wide, ch * s
    pad, label = 18, 22
    cols = min(args.cols, len(items))
    rows = (len(items) + cols - 1) // cols

    W = cols * (iw + pad) + pad
    H = rows * (ih + pad + label) + pad
    sheet = Image.new("RGB", (W, H), (18, 18, 20))
    d = ImageDraw.Draw(sheet)
    f = font(13, True)
    fs = font(11)

    for n, (name, art) in enumerate(items):
        cx = pad + (n % cols) * (iw + pad)
        cy = pad + (n // cols) * (ih + pad + label) + label

        if args.tiles:
            cmap = {" ": C64[args.bg], ".": C64[args.d022],
                    "+": C64[args.d023], "#": C64[args.colram]}
            note = ""
        else:
            cmap, own = sprite_colours(name, args)
            note = f"  col {own}"

        # the game's own background under the art, so a sprite is judged
        # against what it will really sit on
        sheet.paste(C64[args.bg], (cx, cy, cx + iw, cy + ih))
        draw_cell(sheet, cx, cy, art, cmap, s, wide,
                  bg=C64[args.bg] if args.tiles else None)

        if args.grid:
            for gx in range(len(art[0]) + 1):
                d.line([(cx + gx * s * wide, cy),
                        (cx + gx * s * wide, cy + ih)], fill=(60, 60, 66))
            for gy in range(len(art) + 1):
                d.line([(cx, cy + gy * s), (cx + iw, cy + gy * s)],
                       fill=(60, 60, 66))

        d.text((cx, cy - label + 2), name, font=f, fill=(230, 230, 235))
        if note:
            d.text((cx + d.textlength(name, font=f), cy - label + 4),
                   note, font=fs, fill=(130, 130, 140))

    sheet.save(args.out)
    print(f"{len(items)} items -> {args.out}  ({W}x{H})")


if __name__ == "__main__":
    main()
