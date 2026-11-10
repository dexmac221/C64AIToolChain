#!/usr/bin/env python3
"""
assetgen.py - asset pipeline for FULGOR.

Everything the game shows is described here and compiled to data:

  assets.s     charset, sprites, metatile tables, the level map, row tables
  assets.inc   constants for the assembly side
  assets.h     the same constants for C

  ./assetgen.py                 build the data
  ./assetgen.py --preview DIR   also render PNGs (map overview, close-ups,
                                sprite sheet) in the colours the game uses

Inks of a multicolour character cell (4 wide pixels x 8 rows):
  0 background ($D021, set per world row: the sky gradient)
  1 $D022 = brown      2 $D023 = light red   3 colour RAM, one per world row

Colour RAM cannot be double-buffered, so colour belongs to the world ROW,
not to the cell (IRON VEIN measured why). The level is banded on purpose:
the surface rows are green, the caves yellow, the crystal gallery cyan.

Terrain is not drawn tile by tile. One seamless 32x32 rock texture is
generated (stones as Voronoi cells, lit from the top left), and the 16
edge variants are cut out of it with outline, grass and shadow rules; the
level only says "rock here" and the exposed sides pick the variant.
"""

import math
import os
import sys

MAP_W, MAP_H = 128, 40          # metatiles (32x32 pixels each)
MC1, MC2 = 9, 10                # $D022 brown, $D023 light red
FIRST_TILE = 64                 # chars 0..63 are the ROM font

PAL = [(0, 0, 0), (255, 255, 255), (104, 55, 43), (112, 164, 178),
       (111, 61, 134), (88, 141, 67), (53, 40, 121), (184, 199, 111),
       (111, 79, 37), (67, 57, 0), (154, 103, 89), (68, 68, 68),
       (108, 108, 108), (154, 210, 132), (108, 94, 181), (149, 149, 149)]


def h2(x, y, s=0):
    h = (x * 374761393 + y * 668265263 + s * 362437) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFF


# ------------------------------------------------------------- terrain
SEEDS = [(3.0, 4.0), (11.0, 7.5), (5.5, 15.5), (13.5, 20.0), (4.0, 27.0),
         (11.5, 30.5)]


def rock_px(x, y):
    """One pixel of the seamless stone texture (16 wide pixels x 32)."""
    best = []
    for sx, sy in SEEDS:
        dx = ((x + 0.5 - sx + 8) % 16) - 8
        dy = ((y + 0.5 - sy + 16) % 32) - 16
        best.append((math.hypot(dx * 2, dy), dx * 2, dy))
    best.sort()
    d1, dx, dy = best[0]
    e = best[1][0] - d1                 # distance to the cell border
    if e < 1.6:
        return 1                        # the crevice between two stones
    lit = -(dx * 0.6 + dy)              # > 0 towards the top (and left)
    if e < 3.6 and lit > 3.0:
        return 3                        # lit rim
    if e < 4.4 and lit < -3.5 and ((x + y) & 1 or e < 3.0):
        return 1                        # shadowed underside, dithered out
    if h2(x, y, 7) % 41 == 0:
        return 1                        # a pock mark
    return 2


TOP_P = [0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0]
BOT_P = [1, 2, 2, 3, 2, 1, 0, 1, 2, 3, 3, 2, 1, 1, 0, 0]
LEFT_P = [0, 0, 1, 1, 1, 0, 0, 0, 1, 2, 2, 1, 0, 0, 1, 1,
          1, 0, 0, 1, 1, 2, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0]
RIGHT_P = [0, 1, 1, 0, 0, 0, 1, 2, 2, 1, 1, 0, 0, 1, 1, 1,
           0, 0, 1, 2, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0]
T, B, L, R = 1, 2, 4, 8


def terrain(bits):
    t = [[rock_px(x, y) for x in range(16)] for y in range(32)]
    for y in range(32):
        for x in range(16):
            out = False
            edge = False
            if bits & L:
                xl = LEFT_P[y]
                out |= x < xl
                edge |= x == xl
                if x == xl + 1 and t[y][x] == 2 and h2(x, y, 3) % 3:
                    t[y][x] = 3          # the lit face
            if bits & R:
                xr = 15 - RIGHT_P[y]
                out |= x > xr
                edge |= x == xr
                if x == xr - 1 and t[y][x] == 3:
                    t[y][x] = 2          # no highlight on the shadowed face
            if bits & B:
                yb = 31 - BOT_P[x]
                out |= y > yb
                edge |= y == yb
                if y == yb - 1 and t[y][x] == 3:
                    t[y][x] = 2
            if bits & T:
                yt = TOP_P[x]
                hang = 3 + (h2(x, 0, 13) % 3 == 0) + (h2(x, 0, 14) % 5 == 0)
                out |= y < yt
                if yt <= y < yt + hang:
                    t[y][x] = 3          # grass, with blades hanging down
                    edge = False
                elif y == yt + hang:
                    t[y][x] = 1          # the soil line under it
                elif y == yt + hang + 1 and h2(x, y, 5) % 2:
                    t[y][x] = 1
            if edge:
                t[y][x] = 1
            if out:
                t[y][x] = 0
    return t


# -------------------------------------------------------- hand-drawn art
INK = {' ': 0, '.': 1, 'o': 2, '#': 3}


def art(rows, w=16, h=32, ox=0, oy=0):
    """Place a small drawing on an empty 16x32 metatile."""
    t = [[0] * w for _ in range(h)]
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c != ' ':
                t[oy + j][ox + i] = INK[c]
    return t


FERN = [
    "       #        ",
    "   #   ##    #  ",
    "   ##  #.#  ##  ",
    " #  #. ##. ##.  ",
    " ##  #.#. ##. # ",
    "  ##.##.#.#. ## ",
    "  .##.#.##..##. ",
    "   .#.##.#.##.  ",
    "    .#.#.##..   ",
    "     .#.##..    ",
    "      .##.      ",
    "      .#.       ",
]
BUSH = [
    "     ###        ",
    "   ##.###  ##   ",
    "  #.####.####.  ",
    " ###.##.###.##. ",
    " #.####.#.####. ",
    "###.#.####.#.##.",
    "#.###.##.###.#..",
    ".#.#.##.#.#.#.. ",
]
MUSH = [
    "     oooo       ",
    "   oo#ooooo     ",
    "  o##ooo.ooo    ",
    "  ooooo.o.oo    ",
    "  ..........    ",
    "     .oo.       ",
    "     .oo.       ",
    "     .oo.       ",
    "    ..oo..      ",
]
STALAC = [
    ".oooo.  .ooo.   ",
    ".#ooo.  .#oo.   ",
    " .#oo.  .#o.    ",
    " .#oo.   .o.    ",
    "  .#o.   .o.    ",
    "  .#o.    .     ",
    "  .#.           ",
    "   .o.          ",
    "   .o.          ",
    "    .           ",
]
STALAG = [
    "          .     ",
    "         .#.    ",
    "   .     .#.    ",
    "  .#.   .#o.    ",
    "  .#.   .#o.    ",
    " .#o.   .#oo.   ",
    " .#oo. .#ooo.   ",
    ".#ooo. .#oooo.  ",
]
CRYSTAL = [
    "        .       ",
    "       .#.      ",
    "  .    .#.      ",
    " .#.  .##.   .  ",
    " .#.  .##.  .#. ",
    ".##. .###.  .#. ",
    ".##. .##o. .##. ",
    ".#o. .##o. .#o. ",
    ".#o..###o. .#o. ",
    ".#o..##oo..##o. ",
    ".oo..#ooo..#oo. ",
    "................",
]
SPIKES = [
    " .   .   .   .  ",
    " #   #   #   #  ",
    ".#. .#. .#. .#. ",
    ".#. .#. .#. .#. ",
    ".#o..#o..#o..#o.",
    ".#o..#o..#o..#o.",
    "oooooooooooooooo",
    "................",
]
DIAMOND = [
    "   ......   ",
    "  .######.  ",
    " .##.##.##. ",
    ".##.####.##.",
    ".#o######o#.",
    " .oo####oo. ",
    "  .oo##oo.  ",
    "   .oooo.   ",
    "    .oo.    ",
    "     ..     ",
]
POWER = [
    "  ........  ",
    " .oooooooo. ",
    ".o########o.",
    ".o#......#o.",
    ".o#.####.#o.",
    ".o#.#..#.#o.",
    ".o#.####.#o.",
    ".o#.#....#o.",
    ".o#.#....#o.",
    ".o########o.",
    " .oooooooo. ",
    "  ........  ",
]
LAMP = [
    "   .oo.   ",
    "   .oo.   ",
    "  ......  ",
    " .######. ",
    " .#oooo#. ",
    " .######. ",
    "  ......  ",
]


def trunk():
    t = [[0] * 16 for _ in range(32)]
    for y in range(32):
        for x in range(5, 11):
            v = 2
            if x == 5 or x == 10:
                v = 1
            elif x == 6:
                v = 3 if (y % 8) not in (3, 4) else 2
            elif x == 9 and y % 2:
                v = 1
            if y % 8 == 4 and 5 < x < 10:
                v = 1                   # bark rings
            t[y][x] = v
    return t


def crown():
    """A tree crown three metatiles wide: leaf mass with a ragged rim."""
    img = [[0] * 48 for _ in range(32)]
    for y in range(32):
        for x in range(48):
            nx = (x + 0.5 - 24) / 23.0
            ny = (y + 0.5 - 17) / 14.5
            d = nx * nx + ny * ny
            rag = (h2(x // 2, y // 2, 11) % 100) / 100.0 * 0.28
            if d > 1.0 - rag:
                continue
            v = 3
            shade = nx * 0.55 + ny * 0.85
            if shade > 0.42 and (x + y) & 1:
                v = 1
            if shade > 0.75:
                v = 1
            if h2(x // 2, y // 3, 5) % 9 == 0:
                v = 1                   # leaf gaps
            if h2(x, y, 17) % 37 == 0 and shade < 0.3:
                v = 2                   # fruit
            img[y][x] = v
    for x in range(21, 27):             # the trunk comes up into it
        for y in range(24, 32):
            img[y][x] = 1 if x in (21, 26) else 2
    return [[r[i * 16:(i + 1) * 16] for r in img] for i in range(3)]


def platform(kind):
    """A stone ledge one character thick: 'l'eft end, 'm'iddle, 'r'ight
    end or 's'ingle. Solid only from above."""
    t = [[0] * 16 for _ in range(32)]
    for x in range(16):
        if kind in 'ls' and x == 0:
            continue
        if kind in 'rs' and x == 15:
            continue
        cap = (kind in 'ls' and x == 1) or (kind in 'rs' and x == 14)
        col = [3, 2, 2, 2, 2, 1, 0, 0]
        if x % 8 == 3:
            col = [3, 2, 1, 2, 1, 1, 1, 0]      # a joint
        elif x % 8 == 7:
            col = [3, 2, 2, 2, 1, 1, 0, 0]
        if cap:
            col = [1, 1, 1, 1, 1, 0, 0, 0]
        for y in range(8):
            t[y][x] = col[y]
        if x % 8 == 3 and not cap:
            t[6][x] = 1                         # a strut stub below
    return t


def backwall(v):
    """Dark masonry behind the action: brown lines on the background.
    Strictly periodic, so the two variants cost a handful of chars."""
    t = [[0] * 16 for _ in range(32)]
    for y in range(32):
        for x in range(16):
            course = y // 8
            off = 4 if (course + v) & 1 else 0
            if y % 8 == 7 or (x + off) % 8 == 7:
                t[y][x] = 1
            elif y % 8 == 0 and (x + off) % 8 == 0:
                t[y][x] = 1             # a chipped corner
    return t


def block():
    """A riveted copper block from the old machines."""
    t = [[2] * 16 for _ in range(32)]
    for y in range(32):
        for x in range(16):
            if x in (0, 15) or y in (0, 31):
                t[y][x] = 1
            elif x == 1 or y == 1:
                t[y][x] = 3
            elif x == 14 or y == 30:
                t[y][x] = 1
            elif (x in (3, 12)) and (y in (4, 5, 26, 27)):
                t[y][x] = 3
            elif 5 <= x <= 10 and 9 <= y <= 22:
                edge = x in (5, 10) or y in (9, 22)
                t[y][x] = 1 if edge else (2 if (y - 9) % 4 else 1)
    return t


def pillar():
    t = [[0] * 16 for _ in range(32)]
    for y in range(32):
        for x in range(4, 12):
            v = 1
            if x in (6, 9) and y % 16 not in (0, 15):
                v = 2
            if y % 16 in (0, 15):
                v = 2 if 4 < x < 11 else 1
            t[y][x] = v
    return t


def lava(top, phase):
    """Molten rock, periodic 8 wide pixels x 16 rows so it costs few
    chars; the surface row has two phases for the animation."""
    t = [[2] * 16 for _ in range(32)]
    for y in range(32):
        for x in range(16):
            u, w = x % 8, y % 16
            v = 2
            if (u + w // 2) % 8 in (0, 1) and w % 4 < 2:
                v = 3                   # bright veins drifting down-right
            if (u * 3 + w) % 16 == 5 or (u, w) in ((6, 2), (2, 10)):
                v = 1                   # crust
            t[y][x] = v
    if top:
        for x in range(16):
            crest = 2 + (1, 2, 2, 1, 0, 0, 0, 1)[(x + phase * 4) % 8]
            for y in range(8):
                if y < crest:
                    t[y][x] = 0
                elif y < crest + 2:
                    t[y][x] = 3
    return t


def star(v):
    t = [[0] * 16 for _ in range(32)]
    for (x, y, c) in [[(3, 5, 3), (11, 20, 2), (7, 28, 1)],
                      [(12, 9, 3), (2, 17, 1), (9, 25, 2), (5, 2, 1)]][v]:
        t[y][x] = c
    if v == 0:
        t[4][3] = t[6][3] = 1
    return t


EMPTY = [[0] * 16 for _ in range(32)]

# --------------------------------------------------------- the metatiles
# name -> (pixels, solid rows, one-way rows, flags). A "rows" value is four
# 4-bit masks, one per character row of the metatile, bit 3 = left cell.
F_HAZARD, F_GEM, F_POWER = 1, 2, 4
FULL = (15, 15, 15, 15)
NONE = (0, 0, 0, 0)
METATILES = []
MT = {}


def add(name, px, solid=NONE, oneway=NONE, flags=0):
    MT[name] = len(METATILES)
    METATILES.append((name, px, solid, oneway, flags))


add("AIR", EMPTY)
for bits in range(16):
    add("ROCK%X" % bits, terrain(bits), FULL)
for k in "smlr":
    add("PLAT_" + k.upper(), platform(k), NONE, (15, 0, 0, 0))
add("FERN", art(FERN, oy=20))
add("BUSH", art(BUSH, oy=24))
add("MUSH", art(MUSH, oy=23, ox=2))
add("STALAC", art(STALAC))
add("STALAG", art(STALAG, oy=24))
add("CRYSTAL", art(CRYSTAL, oy=20))
add("SPIKES", art(SPIKES, oy=24), flags=F_HAZARD)
add("TRUNK", trunk())
for i, c in enumerate(crown()):
    add("CROWN%d" % i, c)
add("WALL0", backwall(0))
add("WALL1", backwall(1))
add("BLOCK", block(), FULL)
add("PILLAR", pillar())
add("LAMP", art(LAMP, ox=3, oy=2))
add("LAVATOP", lava(True, 0), flags=F_HAZARD)
add("LAVA", lava(False, 0), flags=F_HAZARD)
add("GEM", art(DIAMOND, ox=2, oy=11), flags=F_GEM)
add("POWER", art(POWER, ox=2, oy=10), flags=F_POWER)
add("STAR0", star(0))
add("STAR1", star(1))
assert len(METATILES) <= 64, len(METATILES)

# ------------------------------------------------------------- the level
# Built by a small program instead of typed as 5 000 characters: a ground
# profile, caves carved as rectangles, ledges and decorations placed by
# rule. grid[y][x] is a one-letter kind; rock is auto-tiled afterwards.
SKY_ROWS = 18                   # metatile rows 0..17 are open air and sky

# The hero's jump, measured in game.s: 49 pixels up, a little over two
# metatiles along. So every step up is ONE metatile, every gap on the
# level is one metatile (two only when the far side is lower). The
# layout below is written to those rules; shafts are three wide with
# ledges on alternate sides, so one can fall down the middle and climb
# back up the sides.
GROUND = [(0, 2, 5), (2, 15, 14), (15, 18, 13), (18, 21, 12), (21, 24, 13),
          (24, 34, 12), (34, 51, 7), (51, 57, 15), (57, 76, 10),
          (76, 78, 11), (78, 80, 13), (80, 101, 14), (101, 104, 13),
          (104, 126, 12), (126, 128, 4)]
CAVES = [
    # the crystal gallery under the canyon
    (40, 21, 71, 26), (42, 20, 50, 21), (56, 20, 68, 21), (44, 26, 52, 27),
    (60, 26, 66, 27),
    (53, 15, 56, 21),           # the shaft from the canyon floor
    # the east tunnel back up to the lowlands
    (71, 23, 95, 25), (78, 22, 84, 23), (88, 25, 93, 26),
    (95, 14, 98, 25),           # the big shaft
    # down to the furnace
    (45, 27, 48, 31),
    (22, 31, 60, 37), (24, 30, 34, 31), (40, 30, 44, 31), (50, 30, 58, 31),
    (20, 31, 22, 34), (60, 31, 62, 34),     # an alcove at each end
]
PLATS = [
    # the climb to the high plateau
    (26, 11, 2), (29, 10, 2), (26, 9, 2), (29, 8, 2), (32, 8, 1),
    # across the canyon, and out of it
    (52, 8, 2), (55, 9, 2),
    (55, 14, 2), (52, 13, 2), (55, 12, 2), (52, 11, 2), (55, 10, 2),
    # the canyon shaft
    (53, 20, 1), (55, 19, 1), (53, 18, 1), (55, 17, 1), (53, 16, 1),
    # the ruins
    (62, 7, 3), (66, 6, 3), (70, 7, 2),
    # the lowlands
    (89, 11, 2), (92, 10, 2),
    # the big shaft
    (95, 23, 1), (97, 22, 1), (95, 21, 1), (97, 20, 1), (95, 19, 1),
    (97, 18, 1), (95, 17, 1), (97, 16, 1), (95, 15, 1),
    # the crystal gallery
    (42, 25, 2), (45, 24, 2), (48, 23, 3), (57, 25, 2), (60, 24, 2),
    (63, 23, 3), (67, 24, 2),
    # the furnace shaft, and the ledges over the lava
    (47, 33, 1), (45, 32, 1), (47, 31, 1), (45, 30, 1), (47, 29, 1),
    (45, 28, 1),
    (44, 34, 5), (40, 33, 3), (35, 34, 3), (30, 33, 4), (24, 34, 4),
    (50, 33, 3), (55, 34, 4),
]
BLOCKS = [(60, 9), (61, 9), (61, 8), (73, 9), (74, 9), (86, 13), (87, 13),
          (87, 12), (110, 11), (118, 11)]
PILLARS = [(59, 9), (59, 8), (59, 7), (64, 9), (64, 8), (69, 9), (69, 8),
           (69, 7), (75, 9), (75, 8), (112, 11), (112, 10), (116, 11),
           (116, 10), (120, 11), (120, 10)]
WALLS = [(43, 21, 50, 25), (57, 21, 62, 24), (64, 22, 70, 26),
         (76, 23, 83, 25), (25, 31, 33, 35), (41, 31, 44, 35),
         (52, 31, 58, 35)]
TREES = [(6, 2), (11, 1), (37, 2), (44, 1), (48, 2), (66, 1),
         (82, 2), (91, 1), (106, 2)]
GEMS = [(26, 8), (27, 8), (30, 7), (52, 7), (63, 6), (67, 5), (68, 5),
        (92, 9), (93, 9), (49, 22), (50, 22), (64, 22), (68, 23), (80, 22),
        (31, 32), (32, 32), (41, 32), (51, 32), (20, 33), (21, 33), (61, 33)]
POWERS = [(71, 6), (46, 23), (36, 33)]
HERO_START = (5, 14)            # metatile column, and the ground row under it
# (kind, column, the ground row it stands on / flies over, patrol in metatiles)
E_CRAWL, E_WASP, E_TURRET = 1, 2, 3
ENEMIES = [
    (E_CRAWL, 9, 14, 4), (E_CRAWL, 25, 12, 5), (E_WASP, 20, 9, 6),
    (E_WASP, 30, 5, 5), (E_CRAWL, 37, 7, 5), (E_CRAWL, 44, 7, 4),
    (E_TURRET, 49, 7, 0), (E_WASP, 52, 5, 4), (E_CRAWL, 51, 15, 1),
    (E_TURRET, 58, 10, 0), (E_CRAWL, 63, 10, 5), (E_WASP, 65, 3, 6),
    (E_TURRET, 72, 10, 0), (E_CRAWL, 81, 14, 4), (E_WASP, 84, 9, 6),
    (E_TURRET, 94, 14, 0), (E_CRAWL, 98, 14, 2), (E_CRAWL, 105, 12, 5),
    (E_WASP, 108, 8, 6), (E_TURRET, 113, 12, 0), (E_CRAWL, 115, 12, 5),
    # the crystal gallery and the tunnel
    (E_CRAWL, 41, 26, 2), (E_WASP, 52, 22, 5), (E_CRAWL, 66, 26, 4),
    (E_TURRET, 77, 25, 0), (E_CRAWL, 84, 25, 3), (E_WASP, 86, 22, 4),
    # the furnace
    (E_WASP, 27, 31, 5), (E_WASP, 38, 31, 5), (E_WASP, 52, 31, 4),
]
EXIT_COL = 122                  # reaching this column ends the level
# places to drop the hero for tests and filming ($0341 = 1..)
SPOTS = [(5, 14), (30, 12), (40, 7), (51, 15), (62, 10), (41, 26),
         (80, 25), (46, 34), (90, 14), (112, 12)]


def build_level():
    g = [[' '] * MAP_W for _ in range(MAP_H)]
    top = [0] * MAP_W
    for x0, x1, row in GROUND:
        for x in range(x0, x1):
            top[x] = row
            for y in range(row, MAP_H):
                g[y][x] = '#'
    for x0, y0, x1, y1 in CAVES:
        for y in range(y0, y1):
            for x in range(x0, x1):
                g[y][x] = ' '
    for x, y, n in PLATS:
        for i in range(n):
            g[y][x + i] = '='
    for x, y in BLOCKS:
        g[y][x] = 'B'
    for x, y in PILLARS:
        if g[y][x] == ' ':
            g[y][x] = 'I'
    # the lava lake at the bottom of the furnace
    for x in range(22, 60):
        g[36][x] = 'V'
        g[35][x] = 'v'
    for x, hgt in TREES:
        r = top[x] - 1
        if any(g[r - k][x] != ' ' for k in range(hgt + 1)):
            continue
        for k in range(hgt):
            g[r - k][x] = '|'
        for i in (-1, 0, 1):
            if g[r - hgt][x + i] == ' ':
                g[r - hgt][x + i] = '012'[i + 1]
    for x, y in GEMS:
        assert g[y][x] == ' ', ("gem in rock", x, y)
        g[y][x] = 'd'
    for x, y in POWERS:
        assert g[y][x] == ' ', ("power in rock", x, y)
        g[y][x] = 'P'

    def solid(x, y):
        return not (0 <= x < MAP_W and 0 <= y < MAP_H) or g[y][x] in '#B'

    for x0, y0, x1, y1 in WALLS:
        for y in range(y0, y1):
            for x in range(x0, x1):
                if g[y][x] == ' ':
                    g[y][x] = 'w' if y & 1 else 'W'

    # decoration by rule
    for y in range(1, MAP_H - 1):
        for x in range(1, MAP_W - 1):
            if g[y][x] != ' ':
                continue
            r = h2(x, y, 99) % 100
            floor = g[y + 1][x] == '#'
            ceil = g[y - 1][x] == '#'
            if y < SKY_ROWS:
                if floor and abs(x - HERO_START[0]) > 1:
                    if r < 16:
                        g[y][x] = 'f'
                    elif r < 28:
                        g[y][x] = 'b'
                    elif r < 33:
                        g[y][x] = 'm'
                elif y < 4 and r < 22:
                    g[y][x] = '*' if r < 11 else '+'
            else:
                crystal_zone = 20 <= y < 28
                if floor and crystal_zone and r < 22:
                    g[y][x] = 'c'
                elif floor and r < 14:
                    g[y][x] = 'i'
                elif ceil and r > 62:
                    g[y][x] = 's'
    return g, top, solid


KIND = {' ': "AIR", 'f': "FERN", 'b': "BUSH", 'm': "MUSH", 's': "STALAC",
        'i': "STALAG", 'c': "CRYSTAL", '^': "SPIKES", '|': "TRUNK",
        '0': "CROWN0", '1': "CROWN1", '2': "CROWN2", 'w': "WALL0",
        'W': "WALL1", 'B': "BLOCK", 'I': "PILLAR", 'l': "LAMP",
        'V': "LAVA", 'v': "LAVATOP", 'd': "GEM", 'P': "POWER",
        '*': "STAR0", '+': "STAR1"}


def tile_level():
    g, top, solid = build_level()
    m = [[0] * MAP_W for _ in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(MAP_W):
            c = g[y][x]
            if c == '#':
                bits = 0
                if not solid(x, y - 1): bits |= T
                if not solid(x, y + 1): bits |= B
                if not solid(x - 1, y): bits |= L
                if not solid(x + 1, y): bits |= R
                m[y][x] = MT["ROCK%X" % bits]
            elif c == '=':
                l = g[y][x - 1] in '=#B'
                r = g[y][x + 1] in '=#B'
                m[y][x] = MT["PLAT_" + ("M" if l and r else "R" if l
                                        else "L" if r else "S")]
            else:
                m[y][x] = MT[KIND[c]]
    return m, g, top


def row_tables():
    """Colour RAM value and background colour of every world char row."""
    col, bg = [], []
    sky = ([0] * 8 + [6] * 10 + [14, 6, 6, 14, 6, 14] + [14] * 12
           + [3, 14, 14, 3, 14, 3] + [3] * 10 + [13, 3, 3, 13, 3, 13]
           + [13] * 20)
    for r in range(MAP_H * 4):
        mr = r // 4
        if mr < 4:
            c = 1                       # high sky: white stars
        elif mr < SKY_ROWS:
            c = 5                       # the surface: green
        elif mr < 20:
            c = 7
        elif mr < 28:
            c = 3                       # the crystal gallery: cyan
        else:
            c = 7                       # the furnace: yellow
        col.append(8 + c)
        bg.append(sky[r] if mr < SKY_ROWS else 0)
    return col, bg


# ------------------------------------------------------------- sprites
# 12 wide pixels x 21 rows. ' ' transparent, '.' $D025 (black outline),
# '#' the sprite's own colour, '+' $D026 (white).
SPR_INK = {' ': 0, '.': 1, '#': 2, '+': 3}

HERO_TOP = [
    "    #####   ",
    "   #++####  ",
    "  ##+######.",
    "  #########.",
    "  ###.......",
    "  ###.+++.+.",
    "  ####......",
    "  ########. ",
    "   ######.  ",
    "    ####.   ",
    "  .######.  ",
    " ##+###.###.",
    "###+###.####",
    "#######.###.",
    "###.##......",
    ".##.##++++++",
    " ##.##......",
    "  ..#####.  ",
    "   ######.  ",
    "   .......  ",
    "   #+####.  ",
]
LEGS_STAND = [
    "   ###.##.  ",
    "   ###.##.  ",
    "   #+#.##.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "   +++.++.  ",
    "   +++.++.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "   #+#.##.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "   ###.##.  ",
    "  ####.###. ",
    "  ####.####.",
    "  ####.####.",
    "  ..........",
]


# The run is four drawings in the standing legs' vocabulary: the near leg
# three pixels wide and bright, the far leg two wide with a shadow line,
# white knee pads, a dark sole. Contact, passing, contact, passing.
LEGS_RUN1 = [                  # near leg reaches forward, far leg pushes
    "   ######   ",
    "   .#####.  ",
    "  .##.####. ",
    "  .##. ###. ",
    " .##.  .###.",
    " .##.   ###.",
    " .##.   +++.",
    " .++.   +++.",
    ".##.    ###.",
    ".##.    ###.",
    ".##.    #+#.",
    ".##.    ###.",
    ".##.    ###.",
    ".##.     ###",
    ".##.     ###",
    ".###.   ####",
    ".###.   ####",
    " ....   ####",
    "        ....",
]
LEGS_RUN2 = [                  # near leg carries, far knee comes through
    "   ######   ",
    "   ###.###. ",
    "   ###. ###.",
    "   #+#.  ##.",
    "   ###. .##.",
    "   ###..##. ",
    "   +++.##.  ",
    "   +++.#.   ",
    "   ###...   ",
    "   ###.     ",
    "   #+#.     ",
    "   ###.     ",
    "   ###.     ",
    "   ###.     ",
    "   ###.     ",
    "  ####.     ",
    "  #####.    ",
    "  #####.    ",
    "  ......    ",
]
LEGS_RUN3 = [                  # far leg reaches forward, near leg pushes
    "   ######   ",
    "  .#####.   ",
    " .###.###.  ",
    " ###. .##.  ",
    ".###.  .##. ",
    ".###    ##. ",
    ".+++    ++. ",
    ".+++    ++. ",
    "###.     ##.",
    "###.     ##.",
    "#+#.     ##.",
    "###.     ##.",
    "###.     ##.",
    "###.     ##.",
    "###.     ##.",
    "####.   .###",
    "####.   .###",
    "....    .###",
    "         ...",
]
LEGS_RUN4 = [                  # far leg carries, near knee comes through
    "   ######   ",
    "   .######. ",
    "   .##.####.",
    "   .##. ###.",
    "   .##..###.",
    "   .##.+++. ",
    "   .##.++.  ",
    "   .##.#.   ",
    "   .##.#.   ",
    "   .##...   ",
    "   .##.     ",
    "   .##.     ",
    "   .##.     ",
    "   .##.     ",
    "   .##.     ",
    "  .###.     ",
    "  .####.    ",
    "  .####.    ",
    "  ......    ",
]
LEGS_JUMP = [                  # knees up
    "   ######   ",
    "  .#######. ",
    " .###.#####.",
    " .##. .####.",
    " .##.  +++. ",
    " .++. .+++. ",
    " .++..###.  ",
    " .##.###.   ",
    " .##.##.    ",
    " .##.##.    ",
    " ###.###.   ",
    " ###.####.  ",
    " ....#####. ",
    "     ...... ",
]
LEGS_FALL = [                  # legs long, a little apart
    "   ######   ",
    "  .##.###.  ",
    "  .##.###.  ",
    " .##. .###. ",
    " .##. .#+#. ",
    " .##.  ###. ",
    " .++.  +++. ",
    " .++.  +++. ",
    " .##.  ###. ",
    " .##.  ###. ",
    " .##.  #+#. ",
    " .##.  ###. ",
    " .##.  ###. ",
    " .##.  ###. ",
    " .##.  ###. ",
    ".###. ####. ",
    ".###. ####. ",
    ".###. ####. ",
    "..... ..... ",
]
LEGS = {"STAND": LEGS_STAND, "RUN1": LEGS_RUN1, "RUN2": LEGS_RUN2,
        "RUN3": LEGS_RUN3, "RUN4": LEGS_RUN4, "JUMP": LEGS_JUMP,
        "FALL": LEGS_FALL}
# on the two contact frames the body sinks a pixel onto the legs
HERO_BOB = {"STAND": 0, "RUN1": 1, "RUN2": 0, "RUN3": 1, "RUN4": 0,
            "JUMP": 0, "FALL": 0}

SHOT = [
    "            ",
    "            ",
    "            ",
    "            ",
    "            ",
    "            ",
    "            ",
    "            ",
    "  .........",
    " .##++++++. ",
    " .##++++++. ",
    "  ......... ",
]
# The enemies are machines that fly low: a hero's gun is 24 pixels off
# the ground and a 21-pixel sprite standing on it would never be hit.
CRAWL1 = [                     # a skimmer, thruster lit
    "", "",
    "    ....    ",
    "  ..####..  ",
    " .###++###. ",
    ".##+####+##.",
    ".#+.####.+#.",
    ".##########.",
    ".#..####..#.",
    " .########. ",
    "  ..####..  ",
    "    ....    ",
    "    .++.    ",
    "   .++++.   ",
    "    .++.    ",
    "     ++     ",
    "     ..     ",
]
CRAWL2 = [                     # thruster low
    "", "",
    "    ....    ",
    "  ..####..  ",
    " .###++###. ",
    ".##+####+##.",
    ".#+.####.+#.",
    ".##########.",
    ".#..####..#.",
    " .########. ",
    "  ..####..  ",
    "    ....    ",
    "    .++.    ",
    "     ++     ",
    "     ..     ",
]
WASP1 = [                      # a flying drone, rotors up
    "", "", "",
    ".++.    .++.",
    " .++.  .++. ",
    "  .++..++.  ",
    "   ......   ",
    "  .######.  ",
    " .##++####. ",
    ".##+..+####.",
    ".##.++.####.",
    ".##+..+####.",
    " .########. ",
    "  .######.  ",
    "   ..##..   ",
    "    .++.    ",
    "     ..     ",
]
WASP2 = [                      # rotors down
    "", "", "",
    "            ",
    "            ",
    "   ......   ",
    ".+++....+++.",
    " .++####++. ",
    " .##++####. ",
    ".##+..+####.",
    ".##.++.####.",
    ".##+..+####.",
    " .########. ",
    "  .######.  ",
    "   ..##..   ",
    "    .++.    ",
    "     ..     ",
]
TURRET = [                     # a hovering gun, a barrel each way
    "    ....    ",
    "   .####.   ",
    "  .##++##.  ",
    "....####....",
    "++++#..#++++",
    "....#..#....",
    "  .######.  ",
    " .########. ",
    " .#+####+#. ",
    ".##########.",
    ".#+######+#.",
    " .########. ",
    "  ........  ",
    "   .+..+.   ",
    "   .+..+.   ",
    "    .  .    ",
]
BULLET = [
    "", "", "", "", "", "", "", "",
    "    ....    ",
    "   .++++.   ",
    "   .+##+.   ",
    "   .++++.   ",
    "    ....    ",
]
BOOM1 = [
    "", "", "", "", "", "",
    "     ++     ",
    "    ++++    ",
    "   ++##++   ",
    "   ++##++   ",
    "    ++++    ",
    "     ++     ",
]
BOOM2 = [
    "", "", "",
    "    #  #    ",
    "   ##++##   ",
    "  #++++++#  ",
    "   ++..++   ",
    "  ++....++  ",
    "  ++....++  ",
    "   ++..++   ",
    "  #++++++#  ",
    "   ##++##   ",
    "    #  #    ",
]
BOOM3 = [
    "",
    "  #      #  ",
    "   #    #   ",
    "    +  +    ",
    "  # +  + #  ",
    "     ++     ",
    " #        # ",
    "            ",
    "            ",
    " #        # ",
    "     ++     ",
    "  # +  + #  ",
    "    +  +    ",
    "   #    #   ",
    "  #      #  ",
]

# The title's letters: a 5x7 face, each dot two wide pixels by three
# rows, the top row of every dot white. Shown as expanded sprites.
FONT57 = {
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
}
TITLE = "FULGOR"


def letter(ch):
    rows = []
    for fr in FONT57[ch]:
        top = " " + "".join("++" if c == "#" else "  " for c in fr) + " "
        mid = " " + "".join("##" if c == "#" else "  " for c in fr) + " "
        rows += [top, mid, mid]
    return rows


HERO_SET = [("TOP", HERO_TOP)] + [(n, LEGS[n]) for n in
                                   ("STAND", "RUN1", "RUN2", "RUN3", "RUN4",
                                    "JUMP", "FALL")]
HERO_NFRM = len(HERO_SET)


def norm(rows):
    rows = [r.ljust(12)[:12] for r in rows]
    return rows + [" " * 12] * (21 - len(rows))


def mirror(rows):
    return [r[::-1] for r in norm(rows)]


SPRITES = ([("HERO_" + n, norm(r)) for n, r in HERO_SET]
           + [("HEROL_" + n, mirror(r)) for n, r in HERO_SET]
           + [("SHOT", norm(SHOT)), ("CRAWL1", norm(CRAWL1)),
              ("CRAWL2", norm(CRAWL2)), ("WASP1", norm(WASP1)),
              ("WASP2", norm(WASP2)), ("TURRET", norm(TURRET)),
              ("BULLET", norm(BULLET)), ("BOOM1", norm(BOOM1)),
              ("BOOM2", norm(BOOM2)), ("BOOM3", norm(BOOM3))]
           + [("LOGO%d" % i, norm(letter(c))) for i, c in enumerate(TITLE)])
SPR_COL = {"HERO": 15, "HEROL": 15, "SHOT": 7, "CRAWL1": 4, "CRAWL2": 4,
           "WASP1": 7, "WASP2": 7, "TURRET": 12, "BULLET": 10, "BOOM1": 7,
           "BOOM2": 7, "BOOM3": 7}


def pack_sprite(rows):
    data = []
    for r in rows:
        bits = 0
        for px in r:
            bits = (bits << 2) | SPR_INK[px]
        data += [(bits >> 16) & 0xFF, (bits >> 8) & 0xFF, bits & 0xFF]
    return data + [0]


# ------------------------------------------------------------ compiling
def compile_chars():
    """Cut every metatile into 16 characters, dropping duplicates."""
    chars = {}                          # bytes -> code
    order = []
    mt_tab = [[0] * 64 for _ in range(16)]
    for i, (name, px, solid, oneway, flags) in enumerate(METATILES):
        for sy in range(4):
            for sx in range(4):
                b = []
                for y in range(8):
                    v = 0
                    for x in range(4):
                        v = (v << 2) | px[sy * 8 + y][sx * 4 + x]
                    b.append(v)
                b = bytes(b)
                if not any(b):
                    code = 32           # the blank
                else:
                    if b not in chars:
                        chars[b] = FIRST_TILE + len(order)
                        order.append(b)
                    code = chars[b]
                mt_tab[sy * 4 + sx][i] = code
    assert len(order) <= 256 - FIRST_TILE, "too many chars: %d" % len(order)
    return order, mt_tab


def bytes_lines(data, per=16):
    out = []
    for o in range(0, len(data), per):
        out.append("    .byte " + ",".join("$%02X" % b for b in data[o:o + per]))
    return "\n".join(out) + "\n"


def emit(outdir):
    chars, mt_tab = compile_chars()
    level, grid, top = tile_level()
    row_col, row_bg = row_tables()
    hx = HERO_START[0] * 32 + 4
    hy = HERO_START[1] * 32 - 40

    consts = [("MAP_W", MAP_W), ("MAP_H", MAP_H),
              ("NUM_MT", len(METATILES)), ("NUM_TILE_CHARS", len(chars)),
              ("HERO_START_X", hx), ("HERO_START_Y", hy),
              ("HERO_NFRM", HERO_NFRM), ("COL_MC1", MC1), ("COL_MC2", MC2),
              ("NUM_SPOTS", len(SPOTS)), ("NUM_SPAWN", len(ENEMIES)),
              ("EXIT_X", EXIT_COL * 32), ("E_CRAWL", E_CRAWL),
              ("E_WASP", E_WASP), ("E_TURRET", E_TURRET),
              ("F_HAZARD", F_HAZARD), ("F_GEM", F_GEM), ("F_POWER", F_POWER)]
    consts += [("MT_" + n, i) for n, i in MT.items()]
    consts += [("SF_" + n, i) for i, (n, _) in enumerate(SPRITES)]
    with open(os.path.join(outdir, "assets.inc"), "w") as f:
        f.write("; generated by assetgen.py - do not edit\n")
        for n, v in consts:
            f.write("%s = %d\n" % (n, v))
    with open(os.path.join(outdir, "assets.h"), "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        for n, v in consts:
            f.write("#define %s %d\n" % (n, v))

    with open(os.path.join(outdir, "assets.s"), "w") as f:
        f.write("; generated by assetgen.py - do not edit\n")
        f.write(".export _mt_tab, _mt_solid, _mt_oneway, _mt_flags\n")
        f.write(".export _level_map, _maprow_lo, _maprow_hi\n")
        f.write(".export _row_col, _row_bg, _row_var\n")
        f.write(".export _spot_xl, _spot_xh, _spot_yl, _spot_yh\n")
        f.write(".export _sp_type, _sp_xl, _sp_xh, _sp_yl, _sp_yh, _sp_len\n\n")
        f.write('.segment "CHARSET"\n')
        f.write("    .res %d, 0          ; the ROM font is copied here\n"
                % (FIRST_TILE * 8))
        for b in chars:
            f.write(bytes_lines(list(b)))
        f.write('\n.segment "SPRITES"\n')
        for name, rows in SPRITES:
            f.write("; %s\n" % name)
            f.write(bytes_lines(pack_sprite(rows)))
        f.write('\n.segment "LEVEL"\n')
        # the sky tables first: the band NMI indexes _row_bg by patching
        # the low byte of an absolute address, so it sits on a page
        f.write("_row_bg:\n" + bytes_lines(row_bg + [row_bg[-1]] * (256 - len(row_bg))))
        var = [1 if len(set(row_bg[r:r + 21])) > 1 else 0 for r in range(len(row_bg))]
        f.write("_row_var:\n" + bytes_lines(var + [0] * 32))
        f.write("_mt_tab:\n")
        for k in range(16):
            f.write(bytes_lines(mt_tab[k]))
        # solidity: index = metatile | (char row within it << 6)
        for label, field in (("_mt_solid", 2), ("_mt_oneway", 3)):
            f.write(label + ":\n")
            for sy in range(4):
                row = [0] * 64
                for i, mt in enumerate(METATILES):
                    row[i] = mt[field][sy]
                f.write(bytes_lines(row))
        f.write("_mt_flags:\n")
        f.write(bytes_lines([mt[4] for mt in METATILES]
                            + [0] * (64 - len(METATILES))))
        f.write("_maprow_lo:\n")
        f.write("    .repeat %d, i\n    .byte <(_level_map + i * %d)\n"
                "    .endrepeat\n" % (MAP_H, MAP_W))
        f.write("_maprow_hi:\n")
        f.write("    .repeat %d, i\n    .byte >(_level_map + i * %d)\n"
                "    .endrepeat\n" % (MAP_H, MAP_W))
        f.write("_row_col:\n" + bytes_lines(row_col + [row_col[-1]] * 32))
        # enemies: where they appear, and how far they patrol
        ex = [(c * 32 + 4) for k, c, r, p in ENEMIES]
        ey = [(r * 32 - (43 if k == E_WASP else 30)) for k, c, r, p in ENEMIES]
        f.write("_sp_type:\n" + bytes_lines([k for k, c, r, p in ENEMIES]))
        f.write("_sp_xl:\n" + bytes_lines([v & 255 for v in ex]))
        f.write("_sp_xh:\n" + bytes_lines([v >> 8 for v in ex]))
        f.write("_sp_yl:\n" + bytes_lines([v & 255 for v in ey]))
        f.write("_sp_yh:\n" + bytes_lines([v >> 8 for v in ey]))
        f.write("_sp_len:\n" + bytes_lines([max(1, min(255, p * 32 - 8)) for k, c, r, p in ENEMIES]))
        f.write("_spot_xl:\n" + bytes_lines([(x * 32 + 4) & 255 for x, y in SPOTS]))
        f.write("_spot_xh:\n" + bytes_lines([(x * 32 + 4) >> 8 for x, y in SPOTS]))
        f.write("_spot_yl:\n" + bytes_lines([(y * 32 - 40) & 255 for x, y in SPOTS]))
        f.write("_spot_yh:\n" + bytes_lines([(y * 32 - 40) >> 8 for x, y in SPOTS]))
        f.write("_level_map:\n")
        for y in range(MAP_H):
            f.write(bytes_lines(level[y], 32))
    print("chars: %d/%d  metatiles: %d  sprites: %d  map: %dx%d"
          % (len(chars), 256 - FIRST_TILE, len(METATILES), len(SPRITES),
             MAP_W, MAP_H))
    return level, grid, row_col, row_bg


# -------------------------------------------------------------- preview
def preview(outdir, level, grid, row_col, row_bg):
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    img = Image.new("RGB", (MAP_W * 32, MAP_H * 32))
    px = img.load()
    for my in range(MAP_H):
        for mx in range(MAP_W):
            t = METATILES[level[my][mx]][1]
            for y in range(32):
                r = my * 4 + y // 8
                inks = (row_bg[r], MC1, MC2, row_col[r] & 7)
                for x in range(16):
                    c = PAL[inks[t[y][x]]]
                    px[mx * 32 + x * 2, my * 32 + y] = c
                    px[mx * 32 + x * 2 + 1, my * 32 + y] = c
    img.save(os.path.join(outdir, "map_full.png"))
    img.resize((MAP_W * 8, MAP_H * 8), Image.LANCZOS).save(
        os.path.join(outdir, "map_overview.png"))
    for name, (x0, y0) in (("view_start", (0, 9)), ("view_plateau", (30, 3)),
                           ("view_ruins", (57, 5)), ("view_cave", (42, 19)),
                           ("view_lava", (24, 30)), ("view_canyon", (46, 10))):
        box = (x0 * 32, y0 * 32, x0 * 32 + 320, y0 * 32 + 200)
        img.crop(box).resize((960, 600), Image.NEAREST).save(
            os.path.join(outdir, name + ".png"))

    # sprite sheet: every frame magnified, plus the hero assembled
    S = 6
    n = len(SPRITES)
    cols = 9
    sheet = Image.new("RGB", (cols * 26 * S, ((n + cols - 1) // cols) * 23 * S
                              + 46 * S), PAL[3])
    sp = sheet.load()

    def draw(rows, ox, oy, colour, bgc=None):
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                v = SPR_INK[ch]
                if v == 0:
                    continue
                c = PAL[(0, 0, colour, 1)[v]]
                for yy in range(S):
                    for xx in range(2 * S):
                        sp[ox + x * 2 * S + xx, oy + y * S + yy] = c
    for i, (name, rows) in enumerate(SPRITES):
        colour = SPR_COL.get(name, SPR_COL.get(name.split("_")[0], 14))
        draw(rows, (i % cols) * 26 * S + S, (i // cols) * 23 * S + S, colour)
    base = ((n + cols - 1) // cols) * 23 * S + 2 * S
    top_rows = dict(SPRITES)["HERO_TOP"]
    for k, leg in enumerate(["STAND", "RUN1", "RUN2", "RUN3", "RUN4",
                             "JUMP", "FALL"]):
        ox = k * 26 * S + S
        bob = HERO_BOB[leg] * S
        draw(dict(SPRITES)["HERO_" + leg], ox, base + 21 * S, 15)
        draw(top_rows, ox, base + bob, 15)
    sheet.save(os.path.join(outdir, "sprites.png"))
    with open(os.path.join(outdir, "level.txt"), "w") as f:
        for r in grid:
            f.write("".join(r) + "\n")
    print("preview ->", outdir)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    lv, gr, rc, rb = emit(here)
    if "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1], lv, gr, rc, rb)
