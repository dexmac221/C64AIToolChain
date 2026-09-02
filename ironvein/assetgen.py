#!/usr/bin/env python3
"""
assetgen.py - asset pipeline for IRON VEIN (8-way scroller spike).

Emits:
  tiles_mc.h    multicolor charset (chars 128..) + char_col[256]
  sprites_mc.h  multicolor sprite frames
  level.h       metatile definitions + the 64x32 metatile map

Multicolor char (4 wide-pixels x 8 rows):
  ' ' = 00 background ($D021)   '.' = 01 shared ($D022)
  '+' = 10 shared ($D023)       '#' = 11 per-char colour RAM

Every character code has ONE colour (char_col). That is what keeps the
colour shadow cheap: a new cell's colour is a table lookup, the rest is
a byte copy. Want the same shape in another colour? Define another char.

Multicolor sprite (12 wide-pixels x 21 rows):
  ' ' = 00 transparent          '.' = 01 shared ($D025)
  '#' = 10 per-sprite colour    '+' = 11 shared ($D026)
"""

MC_TILE = {' ': 0, '.': 1, '+': 2, '#': 3}
MC_SPR = {' ': 0, '.': 1, '#': 2, '+': 3}

# ---------------------------------------------------------------- chars
# (name, colour 0..7, rows).  $D022 = dark grey stone, $D023 = mid grey.
CHARS = [
    ("ROCK", 6, [            # blue-veined dark rock
        "....",
        ".#..",
        "....",
        "..#.",
        "....",
        "#...",
        "....",
        "...#",
    ]),
    ("ROCK2", 6, [
        "....",
        "....",
        ".+..",
        "....",
        "...+",
        "....",
        "+...",
        "....",
    ]),
    ("EDGE_T", 5, [          # mossy top edge
        "####",
        "#+##",
        "++++",
        "....",
        "....",
        ".+..",
        "....",
        "....",
    ]),
    ("EDGE_B", 6, [
        "....",
        "....",
        "....",
        "....",
        "++++",
        ".+.+",
        "    ",
        "    ",
    ]),
    ("EDGE_L", 6, [
        "+...",
        "+...",
        "+...",
        "+.#.",
        "+...",
        "+...",
        "+...",
        "+...",
    ]),
    ("EDGE_R", 6, [
        "...+",
        "...+",
        ".#.+",
        "...+",
        "...+",
        "...+",
        "...+",
        "...+",
    ]),
    ("GIRDER", 7, [          # yellow steel beam
        "####",
        "+..+",
        ".##.",
        "+..+",
        "####",
        "    ",
        "    ",
        "    ",
    ]),
    ("PILLAR", 3, [          # cyan tech column
        "+##+",
        "+..+",
        "+##+",
        "+..+",
        "+##+",
        "+..+",
        "+##+",
        "+..+",
    ]),
    ("ORE", 7, [             # rock with yellow ore
        "....",
        ".##.",
        ".#..",
        "....",
        "...#",
        "..##",
        "....",
        "#...",
    ]),
    ("CRYSTAL", 3, [         # cyan crystals
        "....",
        "..#.",
        ".###",
        "..#.",
        "....",
        "#...",
        "##..",
        "....",
    ]),
    ("SPIKES", 2, [          # red spikes
        "#  #",
        "#  #",
        "## #",
        "####",
        "++++",
        "....",
        "....",
        "....",
    ]),
    ("LIGHT", 1, [           # white lamp
        " ## ",
        "####",
        " ## ",
        " .. ",
        " .. ",
        "....",
        "....",
        "....",
    ]),
    ("GRATE", 5, [           # green grating
        "#.#.",
        ".#.#",
        "#.#.",
        ".#.#",
        "#.#.",
        ".#.#",
        "#.#.",
        ".#.#",
    ]),
    ("STAR", 1, [
        "    ",
        "    ",
        "  # ",
        "    ",
        "    ",
        "    ",
        "    ",
        "    ",
    ]),
    ("BAR", 7, [              # HUD energy segment: hires, 6 pixels wide,
        "    ",               # coloured by the cell, a gap to the next
        "### ",
        "### ",
        "### ",
        "### ",
        "### ",
        "### ",
        "    ",
    ]),
]
CHAR_ID = {name: 128 + i for i, (name, _, _) in enumerate(CHARS)}
CHAR_ID["AIR"] = 32          # ROM space, blank, colour 1

# ------------------------------------------------------------ metatiles
# 4x4 chars, written as 4 rows of 4 short keys
K = {
    ".": "AIR", "R": "ROCK", "r": "ROCK2", "T": "EDGE_T", "B": "EDGE_B",
    "L": "EDGE_L", "Q": "EDGE_R", "G": "GIRDER", "P": "PILLAR", "O": "ORE",
    "C": "CRYSTAL", "S": "SPIKES", "l": "LIGHT", "g": "GRATE", "*": "STAR",
}
METATILES = [
    ("AIR",      ["....", "....", "....", "...."]),
    ("ROCK",     ["RrRr", "rRrR", "RrRO", "rRrR"]),
    ("TOP",      ["TTTT", "RrRr", "rRrR", "RrRr"]),
    ("GIRDER",   ["GGGG", "....", "....", "...."]),
    ("PILLAR",   [".PP.", ".PP.", ".PP.", ".PP."]),
    ("ORE",      ["RORr", "rRrO", "ORrR", "rRrR"]),
    ("CRYSTAL",  ["RrCr", "CRrR", "rrRC", "RCrR"]),
    ("HAZARD",   ["SSSS", "RrRr", "rRrR", "RrRr"]),
    ("WALL_L",   ["LRrR", "LrRr", "LRrR", "LrRr"]),
    ("WALL_R",   ["RrRQ", "rRrQ", "RrRQ", "rRrQ"]),
    ("BOTTOM",   ["RrRr", "rRrR", "RrRr", "BBBB"]),
    ("LAMP",     ["....", ".l..", "....", "...."]),
    ("GRATE",    ["gggg", "gggg", "gggg", "gggg"]),
    ("STARS",    ["*...", "...*", ".*..", "...."]),
]
MT_ID = {name: i for i, (name, _) in enumerate(METATILES)}
SOLID = {"ROCK", "TOP", "GIRDER", "PILLAR", "ORE", "CRYSTAL", "HAZARD",
         "WALL_L", "WALL_R", "BOTTOM", "GRATE"}

MAP_W, MAP_H = 64, 32


# The level, one character per metatile, 64 x 32. Drawn as rock and
# air plus the decorations; the edges (TOP under air, BOTTOM over air,
# WALL_L/WALL_R beside it) are derived, so the drawing stays readable.
# Four colour bands top to bottom: the blue surface and sky, the cyan
# crystal cave, the green halls, the red gallery and the boss arena.
#   . air   R rock   O ore   C crystal   H spikes   G girder   P pillar
#   l lamp  g grate  * stars   L/Q the hard walls at both ends
LEVEL = [
    "L..*............*.............*..........*..........*..........Q",
    "L........*............*......................*............*....Q",
    "L............*.......................*.........................Q",
    "L...........................................................*..Q",
    "L...........................................P..l..P..l..P......Q",
    "LRRRRRRRRRRRRRRRRRR....RRRRRRRRRRRRGGG.GGRRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRR....RRRRRRRRRRRRRRR.RRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRR....RRRRRRRRRRRRRRR.RRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRR....RRRRRRRRRRRRRRR.RRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRR....C...........l.........l..........RRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRR......C.........................C....RRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRR....GGGG............C.......GGGG.....RRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRR..C.......GGGG........GGGG...........RRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRR...........................C......C..RRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRHHHRRRRRRRRRHHRRRRRRRRRR.............RRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR.....GGGG....RRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR.............RRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR.............RRRRRRRRRRRRORQ",
    "LR..............P.......l...P.........P.......l...P........RORRQ",
    "LR............................GGGG.........................RRRRQ",
    "LR........GGGG......GGGG................GGGG...............ORRRQ",
    "LR.........................................................RORRQ",
    "LR....RRRRRRRRRRRRRRRRRRRRHHRRRRRRRRHHRRRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LR....RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LR....RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR...l.........l...RQ",
    "LR....ORRRRRRRRRRRRRRRRRRRRRORRRRRRRRRRRRRRRR.................RQ",
    "LR.........l.........l...........l.......RRRR.C...............RQ",
    "LR..........GGGG......GGGG........GGGG......................C.RQ",
    "LR..............O......................O......................RQ",
    "LRRRRRRRHHHRRRRRRRHHHRRRRRRRRRHHHRRRRRRRRRRRRRRgRRRgRRRgRRRgRRRQ",
    "LRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRQ",
    "LRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRQ",
]

LEVEL_KEY = {
    ".": "AIR", "R": "ROCK", "O": "ORE", "C": "CRYSTAL", "H": "HAZARD",
    "G": "GIRDER", "P": "PILLAR", "l": "LAMP", "g": "GRATE", "*": "STARS",
    "L": "WALL_L", "Q": "WALL_R",
}
HERO_START = (2, 4)              # metatile column and row of the hero's feet
                                 # (standing on the tile below)


def build_map():
    m = [[LEVEL_KEY[c] for c in r] for r in LEVEL]
    assert len(m) == MAP_H and all(len(r) == MAP_W for r in m)

    def is_air(x, y):
        return 0 <= x < MAP_W and 0 <= y < MAP_H and m[y][x] in (
            "AIR", "LAMP", "STARS")
    for y in range(MAP_H):
        for x in range(MAP_W):
            if LEVEL[y][x] != "R":
                continue
            if is_air(x, y - 1):   m[y][x] = "TOP"
            elif is_air(x, y + 1): m[y][x] = "BOTTOM"
            elif is_air(x - 1, y): m[y][x] = "WALL_L"
            elif is_air(x + 1, y): m[y][x] = "WALL_R"
    return m


# -------------------------------------------------------------- sprites
# The hero is drawn facing right; the left-facing set is generated by
# reversing each row (a wide pixel is one character, so a mirror is a
# string reversal). Frame order matters: C indexes RUN1..RUN4 from
# SF_HERO_RUN1, and the left set starts HERO_NFRM frames later.
HERO_FRAMES = [
    ("STAND", [
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "    ######  ",
        "    ##  ##  ",
        "    ##  ##  ",
        "    ##  ##  ",
        "    ##  ##  ",
        "    ##  ##  ",
        "   ...  ... ",
        "   ...  ... ",
    ]),
    ("RUN1", [                       # stride
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "    ######  ",
        "    ######  ",
        "   ##  ##   ",
        "  ##    ##  ",
        " ##      ## ",
        "##        ##",
        "...      ...",
        "            ",
    ]),
    ("RUN2", [                       # legs passing, body up one
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "    ######  ",
        "    ######  ",
        "    ## ##   ",
        "    ## ##   ",
        "   ##  ##   ",
        "   ##  ##   ",
        "  ...  ...  ",
        "            ",
        "            ",
    ]),
    ("RUN3", [                       # the other stride, narrower
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "    ######  ",
        "    ######  ",
        "   ###  ##  ",
        "   ##    ## ",
        "  ##     ## ",
        "  ##      ##",
        " ...      ..",
        "            ",
    ]),
    ("RUN4", [                       # contact, body up one
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "    ######  ",
        "    ######  ",
        "    ##  ##  ",
        "    ##  ##  ",
        "   ##   ##  ",
        "   ##    ## ",
        "  ...    ...",
        "            ",
        "            ",
    ]),
    ("JUMP", [                       # legs tucked
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "   ##..+#   ",
        "   ######   ",
        "    ####    ",
        "   ######   ",
        "  ########  ",
        " ##.####++++",
        " ##.####  + ",
        " ## #####   ",
        "    ######  ",
        "   #######  ",
        "  ##  ###   ",
        "  ##  ##    ",
        " ...  ##    ",
        "      ...   ",
        "            ",
        "            ",
        "            ",
    ]),
]
HERO_NFRM = len(HERO_FRAMES)


def mirror(rows):
    return [r.ljust(12)[::-1] for r in rows]


OTHER_SPRITES = [
    ("DRONE1", [
        "            ",
        "            ",
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "  ###..###  ",
        "  ##.++.##  ",
        "  ##.++.##  ",
        "  ###..###  ",
        "   ######   ",
        "    ####    ",
        "   #    #   ",
        "  #      #  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    ("DRONE2", [
        "            ",
        "            ",
        "            ",
        "            ",
        "    ####    ",
        "   ######   ",
        "  ###..###  ",
        "  ##.++.##  ",
        "  ##.++.##  ",
        "  ###..###  ",
        "   ######   ",
        "    ####    ",
        "     ##     ",
        "     ##     ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    ("SHOT", [                       # a bolt: white core, coloured tail
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "   ##++##   ",
        "   ##++##   ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    ("BOOM1", [                      # a burst that grows and hollows out
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "     ++     ",
        "    ++++    ",
        "   ++##++   ",
        "   ++##++   ",
        "    ++++    ",
        "     ++     ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    ("BOOM2", [
        "            ",
        "            ",
        "            ",
        "    #  #    ",
        "   ##++##   ",
        "  #+++++++# ",
        "   ++..++   ",
        "  ++....++  ",
        "  ++....++  ",
        "   ++..++   ",
        "  #+++++++# ",
        "   ##++##   ",
        "    #  #    ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    ("BOOM3", [
        "            ",
        "  #      #  ",
        "   #    #   ",
        "    .  .    ",
        "  # .  . #  ",
        "     ..     ",
        " #        # ",
        "            ",
        "            ",
        " #        # ",
        "     ..     ",
        "  # .  . #  ",
        "    .  .    ",
        "   #    #   ",
        "  #      #  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
]

# The boss: a 24 x 42 drawing (two sprites wide, two tall) split into
# four frames, plus a second pair of leg frames. C stacks the parts at
# fixed offsets from one origin; the multiplexer sees four objects.
BOSS_BODY = [
    "        ########        ",
    "      ############      ",
    "     ####++++++####     ",
    "    ###+++....+++###    ",
    "    ##++..####..++##    ",
    "    ##+..######..+##    ",
    "    ##++..####..++##    ",
    "    ###+++....+++###    ",
    "     ####++++++####     ",
    "      ############      ",
    "  ....############....  ",
    "  .##################.  ",
    " .####################. ",
    " ######..########..#### ",
    " #####.++.######.++.### ",
    " ######..########..#### ",
    " ###################### ",
    " .####################. ",
    "  .##################.  ",
    "   ..############..     ",
    "   .###..######..###.   ",
]
BOSS_LEGS1 = [
    "   ####  ######  ####   ",
    "  ####   ######   ####  ",
    "  ###    ######    ###  ",
    " ###     ######     ### ",
    " ###     ######     ### ",
    " ##      .####.      ## ",
    " ##      ......      ## ",
    "##       ......       ##",
    "##                    ##",
    "##                    ##",
    "##                    ##",
    "##                    ##",
    "##.                  .##",
    "...                  ...",
]
BOSS_LEGS2 = [
    "   ####  ######  ####   ",
    "  ####   ######   ####  ",
    " ####    ######    #### ",
    "####     ######     ####",
    "##       ######       ##",
    "##       .####.       ##",
    "##       ......       ##",
    "##.      ......      .##",
    "...                  ...",
]


def split_boss(body, legs):
    block = [r.ljust(24) for r in body + legs]
    block += ["            " * 2] * (42 - len(block))
    assert len(block) == 42 and all(len(r) == 24 for r in block)
    tl = [r[:12] for r in block[:21]]
    tr = [r[12:] for r in block[:21]]
    bl = [r[:12] for r in block[21:]]
    br = [r[12:] for r in block[21:]]
    return tl, tr, bl, br


_tl, _tr, _bl1, _br1 = split_boss(BOSS_BODY, BOSS_LEGS1)
_, _, _bl2, _br2 = split_boss(BOSS_BODY, BOSS_LEGS2)
BOSS_SPRITES = [("BOSS_TL", _tl), ("BOSS_TR", _tr), ("BOSS_BL", _bl1),
                ("BOSS_BR", _br1), ("BOSS_BL2", _bl2), ("BOSS_BR2", _br2)]

SPRITES = ([("HERO_" + n, r) for n, r in HERO_FRAMES]
           + [("HEROL_" + n, mirror(r)) for n, r in HERO_FRAMES]
           + OTHER_SPRITES + BOSS_SPRITES)


def pack_mc_tile(rows):
    out = []
    for r in rows:
        r = r.ljust(4)
        b = 0
        for px in r[:4]:
            b = (b << 2) | MC_TILE[px]
        out.append(b)
    return out


def pack_sprite(rows):
    data = []
    for r in rows:
        r = r.ljust(12)
        bits = 0
        for px in r[:12]:
            bits = (bits << 2) | MC_SPR[px]
        data += [(bits >> 16) & 0xFF, (bits >> 8) & 0xFF, bits & 0xFF]
    data.append(0)
    return data


def emit_tiles(path):
    col = [1] * 256                   # text and anything else: hires white
    for i, (name, c, _) in enumerate(CHARS):
        col[128 + i] = 8 + c          # multicolour flag + colour
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        for name, cid in CHAR_ID.items():
            f.write(f"#define T_{name} {cid}\n")
        f.write(f"const unsigned char tile_gfx[{len(CHARS) * 8}] = {{\n")
        for name, _, rows in CHARS:
            f.write("    " + ", ".join(f"0x{b:02X}" for b in pack_mc_tile(rows))
                    + f", /* {name} */\n")
        f.write("};\n")
        f.write("const unsigned char char_col[256] = {\n")
        for o in range(0, 256, 16):
            f.write("    " + ", ".join(str(v) for v in col[o:o+16]) + ",\n")
        f.write("};\n")


def emit_sprites(path, inc):
    with open(inc, "w") as f:
        f.write("; generated by assetgen.py - do not edit\n")
        for i, (name, _) in enumerate(SPRITES):
            f.write(f"SF_{name} = {i}\n")
        f.write(f"HERO_NFRM = {HERO_NFRM}\n")
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        for i, (name, _) in enumerate(SPRITES):
            f.write(f"#define SF_{name} {i}\n")
        f.write(f"#define HERO_NFRM {HERO_NFRM}\n")
        f.write(f"const unsigned char sprite_gfx[{len(SPRITES) * 64}] = {{\n")
        for name, rows in SPRITES:
            data = pack_sprite(rows)
            assert len(data) == 64, name
            f.write(f"    /* {name} */\n")
            for o in range(0, 64, 16):
                f.write("    " + ", ".join(f"0x{b:02X}" for b in data[o:o+16])
                        + ",\n")
        f.write("};\n")


def row_colours():
    """One colour per world character row: the cave brightens towards
    the surface and glows towards the depths. Colour RAM holds only 0-7
    for multicolour chars (bit 3 set), so no grey, brown or orange."""
    rows = MAP_H * 4
    out = []
    for r in range(rows):
        if r < 40:   c = 6        # blue: cold upper rock
        elif r < 64: c = 3        # cyan band where the crystals are
        elif r < 96: c = 5        # green: mossy middle
        else:        c = 2        # red: deep and hot
        out.append(8 + c)
    return out


def emit_level(path):
    m = build_map()
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        f.write(f"#define MAP_W {MAP_W}\n#define MAP_H {MAP_H}\n")
        f.write(f"#define NUM_MT {len(METATILES)}\n")
        f.write(f"#define HERO_START_X {HERO_START[0] * 32 + 10}\n")
        f.write(f"#define HERO_START_Y {(HERO_START[1] + 1) * 32 - 21}\n")
        for name, i in MT_ID.items():
            f.write(f"#define MT_{name} {i}\n")
        f.write(f"const unsigned char mt_chars[{len(METATILES) * 16}] = {{\n")
        for name, rows in METATILES:
            ids = [CHAR_ID[K[k]] for r in rows for k in r]
            f.write("    " + ", ".join(str(v) for v in ids) + f", /* {name} */\n")
        f.write("};\n")
        f.write("const unsigned char mt_solid[NUM_MT] = {\n    "
                + ", ".join("1" if n in SOLID else "0" for n, _ in METATILES)
                + "\n};\n")
        f.write(f"const unsigned char level_map[{MAP_W * MAP_H}] = {{\n")
        for y in range(MAP_H):
            f.write("    " + ", ".join(str(MT_ID[m[y][x]]) for x in range(MAP_W))
                    + ",\n")
        f.write("};\n")
        rc = row_colours()
        f.write(f"const unsigned char row_col[{len(rc)}] = {{\n")
        for o in range(0, len(rc), 16):
            f.write("    " + ", ".join(str(v) for v in rc[o:o+16]) + ",\n")
        f.write("};\n")


if __name__ == "__main__":
    emit_tiles("tiles_mc.h")
    emit_sprites("sprites_mc.h", "sprites.inc")
    emit_level("level.h")
    print(f"chars: {len(CHARS)}  metatiles: {len(METATILES)}  "
          f"sprites: {len(SPRITES)}  map: {MAP_W}x{MAP_H}")
