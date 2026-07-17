#!/usr/bin/env python3
"""
assetgen.py - Unified asset pipeline for ION RIFT.

Generates:
  tiles_mc.h    multicolor/hires custom chars for the scrolling terrain
  sprites_mc.h  multicolor hardware sprite frames

Multicolor char symbols (4 wide-pixels x 8 rows):
  ' ' = 00 background ($D021)
  '.' = 01 shared ($D022)
  '+' = 10 shared ($D023)
  '#' = 11 per-char colour (colour RAM & 7, colour RAM must be >= 8)

Hires char symbols (8 pixels x 8 rows): ' '=0, '#'=1

Multicolor sprite symbols (12 wide-pixels x 21 rows):
  ' ' = 00 transparent
  '.' = 01 shared ($D025)
  '#' = 10 per-sprite colour ($D027+n)
  '+' = 11 shared ($D026)
"""

MC = {' ': 0, '.': 1, '+': 2, '#': 3}

# ---------------------------------------------------------------- tiles

TILES = [
    # 0: FLOOR_TOP - deck plating surface
    ("FLOOR_TOP", "mc", [
        "####",
        "++++",
        ".+.+",
        "....",
        ".  .",
        "....",
        "+.+.",
        "....",
    ]),
    # 1: FLOOR_FILL - hull interior greeble
    ("FLOOR_FILL", "mc", [
        "....",
        ".++.",
        ".  .",
        "....",
        "+..+",
        ".  .",
        ".++.",
        "....",
    ]),
    # 2: CEIL_BOT - ceiling underside
    ("CEIL_BOT", "mc", [
        "....",
        "+.+.",
        "....",
        ".  .",
        "....",
        ".+.+",
        "++++",
        "####",
    ]),
    # 3: CEIL_FILL
    ("CEIL_FILL", "mc", [
        "....",
        ".++.",
        "+..+",
        "....",
        ".  .",
        "....",
        ".++.",
        "....",
    ]),
    # 4: TOWER - structure column
    ("TOWER", "mc", [
        "#..#",
        "#++#",
        "#..#",
        "#..#",
        "#++#",
        "#..#",
        "#..#",
        "#..#",
    ]),
    # 5: TOWER_TOP
    ("TOWER_TOP", "mc", [
        " ## ",
        "####",
        "#++#",
        "#..#",
        "#..#",
        "#++#",
        "#..#",
        "#..#",
    ]),
    # 6: GREEBLE - occasional hull detail
    ("GREEBLE", "mc", [
        "....",
        ".#+.",
        ".++.",
        "....",
        ".+#.",
        ".##.",
        "....",
        "....",
    ]),
    # 7: STAR1 - multicolor sparkle (shared bright colour, static colram)
    ("STAR1", "mc", [
        "    ",
        " +  ",
        "+++ ",
        " +  ",
        "    ",
        "    ",
        "    ",
        "    ",
    ]),
    # 8: STAR2 - multicolor dot
    ("STAR2", "mc", [
        "    ",
        "    ",
        "    ",
        "    ",
        "  + ",
        "    ",
        "    ",
        "    ",
    ]),
    # 9: HUDLINE - hires separator bar
    ("HUDLINE", "hi", [
        "        ",
        "        ",
        "########",
        "########",
        "        ",
        "        ",
        "        ",
        "        ",
    ]),
]

# ---------------------------------------------------------------- sprites

SPRITES = [
    # 0/1: player ship - bigger fuselage, twin fins, white canopy/nose,
    # orange flame gradient (frame B stretches the flame)
    ("SHIP0", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "     ##     ",
        "     ####   ",
        "  ..######+ ",
        ".. #######++",
        "  ..######+ ",
        "     ####   ",
        "     ##     ",
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
    ("SHIP1", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "     ##     ",
        "     ####   ",
        " ...######+ ",
        "...########+",
        " ...######+ ",
        "     ####   ",
        "     ##     ",
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
    # 2/3: drone saucer - white dome, full rim, running lights that
    # alternate between the two frames
    ("DRONE0", [
        "            ",
        "            ",
        "            ",
        "    ++++    ",
        "   ++++++   ",
        "  +######+  ",
        " ########## ",
        "############",
        " .#.####.#. ",
        "  ........  ",
        "   .    .   ",
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
    ("DRONE1", [
        "            ",
        "            ",
        "            ",
        "    ++++    ",
        "   ++++++   ",
        "  +######+  ",
        " ########## ",
        "############",
        " #.#.##.#.# ",
        "  ........  ",
        "  .      .  ",
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
    # 4: dart interceptor - white leading edge, wider delta body
    ("DART", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "  +#        ",
        " ++###      ",
        " +######    ",
        "++##########",
        " +######    ",
        " ++###      ",
        "  +#        ",
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
    # 5: player bolt - white core, orange trail
    ("PBOLT", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "  ..++##    ",
        " ..++####   ",
        "  ..++##    ",
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
        "            ",
    ]),
    # 6: enemy orb - pulsing plasma ball
    ("EORB", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "    ..      ",
        "   .##.     ",
        "  .####.    ",
        "   .##.     ",
        "    ..      ",
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
    # 7/8: explosion - dense core burst, then scattered debris
    ("EXPL0", [
        "            ",
        "            ",
        "            ",
        "            ",
        "   .  + .   ",
        "  + ### +   ",
        "   #####.   ",
        " .########  ",
        "  .######+  ",
        "   #####    ",
        "  + ### .   ",
        "   .  +     ",
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
    ("EXPL1", [
        "            ",
        "            ",
        "            ",
        "  +     .   ",
        " .  + .   + ",
        "   .   +    ",
        " +  . .  .  ",
        ".  + . + .  ",
        "  .  +  .  +",
        " + . +  .   ",
        "  .   . +  .",
        "    + .     ",
        " .    .  +  ",
        "   +    .   ",
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
            b = (b << 2) | MC[px]
        out.append(b)
    return out


def pack_hi_tile(rows):
    out = []
    for r in rows:
        r = r.ljust(8)
        b = 0
        for px in r[:8]:
            b = (b << 1) | (1 if px == '#' else 0)
        out.append(b)
    return out


def pack_sprite(rows):
    """12 wide-pixels x 21 rows -> 63 bytes + pad."""
    data = []
    for r in rows:
        r = r.ljust(12)
        bits = 0
        for px in r[:12]:
            bits = (bits << 2) | MC[px]
        data += [(bits >> 16) & 0xFF, (bits >> 8) & 0xFF, bits & 0xFF]
    data.append(0)
    return data


def emit_tiles(path):
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        f.write(f"#define TILE_COUNT {len(TILES)}\n")
        f.write("#define TILE_BASE 128\n")
        for i, (name, _, _) in enumerate(TILES):
            f.write(f"#define T_{name} {128 + i}\n")
        f.write(f"const unsigned char tile_gfx[{len(TILES) * 8}] = {{\n")
        for name, kind, rows in TILES:
            data = pack_mc_tile(rows) if kind == "mc" else pack_hi_tile(rows)
            f.write("    " + ", ".join(f"0x{b:02X}" for b in data)
                    + f", /* {name} */\n")
        f.write("};\n")


def emit_sprites(path):
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        f.write(f"#define SPRITE_FRAMES {len(SPRITES)}\n")
        for i, (name, _) in enumerate(SPRITES):
            f.write(f"#define SF_{name} {i}\n")
        f.write(f"const unsigned char sprite_gfx[{len(SPRITES) * 64}] = {{\n")
        for name, rows in SPRITES:
            data = pack_sprite(rows)
            assert len(data) == 64, name
            f.write(f"    /* {name} */\n")
            for o in range(0, 64, 16):
                f.write("    " + ", ".join(f"0x{b:02X}"
                        for b in data[o:o + 16]) + ",\n")
        f.write("};\n")


if __name__ == "__main__":
    emit_tiles("tiles_mc.h")
    emit_sprites("sprites_mc.h")
    print(f"tiles_mc.h: {len(TILES)} tiles, sprites_mc.h: "
          f"{len(SPRITES)} sprite frames")
