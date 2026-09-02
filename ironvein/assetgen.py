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
import random

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


def build_map(seed=7):
    """A cave with a rolling floor, a ceiling, hanging pillars, floating
    girders and pockets of ore and crystal. Procedural for the spike; the
    game gets a hand-designed map through the same format."""
    rnd = random.Random(seed)
    m = [["AIR"] * MAP_W for _ in range(MAP_H)]
    # the view is 22 character rows = 5.5 metatiles; keep the air band
    # narrower than that so rock is always on screen
    floor = 18
    ceil = 12
    for x in range(MAP_W):
        if x % 5 == 0:
            floor = max(16, min(21, floor + rnd.choice((-1, 0, 0, 1))))
            ceil = max(9, min(13, ceil + rnd.choice((-1, 0, 0, 1))))
        for y in range(MAP_H):
            if y >= floor:
                m[y][x] = "TOP" if y == floor else rnd.choice(
                    ("ROCK", "ROCK", "ROCK", "ORE", "CRYSTAL"))
            elif y <= ceil:
                m[y][x] = "BOTTOM" if y == ceil else "ROCK"
            elif y < ceil + 5 and rnd.random() < 0.02:
                m[y][x] = "STARS"
    # hanging pillars from the ceiling
    for x in range(4, MAP_W - 4, rnd.choice((7, 9, 11))):
        top = next(y for y in range(MAP_H) if m[y][x] == "AIR")
        for y in range(top, top + rnd.randint(2, 4)):
            if m[y][x] == "AIR":
                m[y][x] = "PILLAR"
    # floating girders
    for _ in range(40):
        x = rnd.randint(2, MAP_W - 6)
        y = rnd.randint(ceil + 1, 20)
        w = rnd.randint(2, 4)
        if all(m[y][x + i] == "AIR" and m[y + 1][x + i] == "AIR"
               for i in range(w)):
            for i in range(w):
                m[y][x + i] = "GIRDER"
    # some spikes on the floor, a lamp here and there
    for x in range(MAP_W):
        for y in range(1, MAP_H):
            if m[y][x] == "TOP" and rnd.random() < 0.08:
                m[y][x] = "HAZARD"
            if m[y][x] == "AIR" and m[y - 1][x] in ("ROCK", "BOTTOM") \
                    and rnd.random() < 0.06:
                m[y][x] = "LAMP"
    # hard walls at both ends
    for y in range(MAP_H):
        m[y][0] = "WALL_L"
        m[y][MAP_W - 1] = "WALL_R"
    return m


# -------------------------------------------------------------- sprites
SPRITES = [
    ("HERO", [
        "            ",
        "     +      ",
        "    ###     ",
        "   #####    ",
        "   #..##    ",
        "   #####    ",
        "    ###     ",
        "  #######+  ",
        " ########+  ",
        " ##.#####+  ",
        "  ######+   ",
        "   #####    ",
        "   ## ##    ",
        "   ## ##    ",
        "   ## ##    ",
        "  ### ###   ",
        "  ##   ##   ",
        "  ##   ##   ",
        " ...   ...  ",
        "            ",
        "            ",
    ]),
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
    ("SHOT", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "   ++##     ",
        "   ++##     ",
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
]


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


def emit_sprites(path):
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        for i, (name, _) in enumerate(SPRITES):
            f.write(f"#define SF_{name} {i}\n")
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
    emit_sprites("sprites_mc.h")
    emit_level("level.h")
    print(f"chars: {len(CHARS)}  metatiles: {len(METATILES)}  "
          f"sprites: {len(SPRITES)}  map: {MAP_W}x{MAP_H}")
