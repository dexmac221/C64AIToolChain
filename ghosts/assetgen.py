#!/usr/bin/env python3
"""
assetgen.py - asset pipeline for GHOST KEEP (Ghosts'n Goblins tribute).

Emits:
  tiles_mc.h    multicolor charset for the graveyard
  sprites_mc.h  multicolor sprite frames (Arthur, zombies, weapons)
  level.h       the hand-designed level 1 map

Multicolor char (4 wide-pixels x 8 rows):
  ' ' = 00 background ($D021)   '.' = 01 shared ($D022)
  '+' = 10 shared ($D023)       '#' = 11 per-char colour RAM

Multicolor sprite (12 wide-pixels x 21 rows):
  ' ' = 00 transparent          '.' = 01 shared ($D025)
  '#' = 10 per-sprite colour    '+' = 11 shared ($D026)
"""

# two different bit orders, one per VIC mode
# Multicolor TEXT: 00 bg, 01 $D022, 10 $D023, 11 colour RAM
MC_TILE = {' ': 0, '.': 1, '+': 2, '#': 3}
# Multicolor SPRITE: 00 transparent, 01 $D025, 10 sprite colour, 11 $D026
# ('#' is the body, so it must land on the per-sprite colour, not on a
#  shared register - otherwise every sprite on screen is the same colour)
MC_SPR = {' ': 0, '.': 1, '#': 2, '+': 3}

# ---------------------------------------------------------------- tiles

TILES = [
    # 0 GRASS - green turf (per-char) over solid brown earth
    ("GRASS", [
        "#+#+",
        "####",
        "#++#",
        "++++",
        "++#+",
        "++++",
        "+#++",
        "++++",
    ]),
    # 1 DIRT - solid earth: foreground pixels, so it buries a sprite
    ("DIRT", [
        "++++",
        "++++",
        "+#++",
        "++++",
        "++++",
        "+++#",
        "++++",
        "++++",
    ]),
    # 2 STONE - buried masonry showing through the soil
    ("STONE", [
        "+##+",
        "#++#",
        "#++#",
        "+##+",
        "++++",
        "+##+",
        "#++#",
        "+##+",
    ]),
    # 3 TOMB_TOP - rounded headstone, carved cross
    ("TOMB_TOP", [
        " .. ",
        "....",
        ". ..",
        ". . ",
        ".  .",
        ". ..",
        "....",
        "....",
    ]),
    # 4 TOMB_BOT - base sunk into the turf
    ("TOMB_BOT", [
        "....",
        ".  .",
        "....",
        "....",
        ".  .",
        "....",
        "....",
        " .. ",
    ]),
    # 5 CROSS - grave marker
    ("CROSS", [
        " .. ",
        " .. ",
        "....",
        "....",
        " .. ",
        " .. ",
        " .. ",
        " .. ",
    ]),
    # 6 TRUNK - dead tree, solid enough to stand behind
    ("TRUNK", [
        " ++ ",
        " ++ ",
        " +++",
        "+++ ",
        " ++ ",
        " ++ ",
        "+++ ",
        " ++ ",
    ]),
    # 7 BRANCH - bare crooked branches
    ("BRANCH", [
        "+  +",
        " + +",
        " +++",
        "++ +",
        " ++ ",
        " ++ ",
        " ++ ",
        " ++ ",
    ]),
    # 8 FENCE - iron railing
    ("FENCE", [
        "    ",
        ".  .",
        "....",
        ".  .",
        ".  .",
        "....",
        ".  .",
        ".  .",
    ]),
    # 9 MOON - pale disc
    ("MOON", [
        " .. ",
        "....",
        "....",
        "....",
        "....",
        "....",
        "....",
        " .. ",
    ]),
    # 10 HUDBAR - separator under the status line
    ("HUDBAR", [
        "    ",
        "    ",
        "....",
        "....",
        "    ",
        "    ",
        "    ",
        "    ",
    ]),
    # 11 FAR_TOMB - headstone on the horizon
    ("FAR_TOMB", [
        "    ",
        "    ",
        "    ",
        " ## ",
        " ## ",
        " ## ",
        " ## ",
        "####",
    ]),
    # 12 FAR_CROSS - distant grave marker
    ("FAR_CROSS", [
        "    ",
        "    ",
        " #  ",
        " #  ",
        "### ",
        " #  ",
        " #  ",
        " #  ",
    ]),
    # 13 FAR_TREE - skeletal tree far away
    ("FAR_TREE", [
        "    ",
        "# # ",
        "#+# ",
        " #  ",
        " #  ",
        " #  ",
        " #  ",
        " #  ",
    ]),
    # 14 RIDGE - distant land, solid dark mass
    ("RIDGE", [
        "# # ",
        " # #",
        "# # ",
        " # #",
        "# # ",
        " # #",
        "# # ",
        " # #",
    ]),
    # 15 RIDGE_TOP - its crest, with a little scrub
    ("RIDGE_TOP", [
        "    ",
        "  # ",
        " # #",
        "# # ",
        " # #",
        "# # ",
        " # #",
        "# # ",
    ]),
    # 16 RIDGE_BASE - the foot of the far land, thinning into the haze
    # so the band does not end on a hard straight line
    ("RIDGE_BASE", [
        "# # ",
        " # #",
        "#   ",
        "  # ",
        "    ",
        " #  ",
        "    ",
        "    ",
    ]),
    # 17 STAR - a cold point of light
    ("STAR", [
        "    ",
        "    ",
        "  . ",
        "    ",
        "    ",
        "    ",
        "    ",
        "    ",
    ]),
]

# ---------------------------------------------------------------- sprites
# Arthur is drawn armoured; the same shapes serve the stripped knight
# with a different sprite colour, exactly like the arcade gag.

SPRITES = [
    # 0 STAND - lance shouldered, facing right
    ("ART_STAND", [
        "            ",
        "            ",
        "     ++     ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "   ######   ",
        "  ###++###  ",
        "  ###++###  ",
        "  ########  ",
        "   ##  ##   ",
        "   ##  ##   ",
        "   ##  ##   ",
        "  ###  ###  ",
        "  ###  ###  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 1 RUN A - leading leg forward
    ("ART_RUN1", [
        "            ",
        "            ",
        "     ++     ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "   ######   ",
        "  ###++###  ",
        " ####++###  ",
        "  ########  ",
        "   ##  ##   ",
        "  ##    ##  ",
        " ##      ## ",
        "###      ###",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 2 RUN B - legs crossing
    ("ART_RUN2", [
        "            ",
        "            ",
        "     ++     ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "   ######   ",
        "  ###++###  ",
        "  ###++#### ",
        "  ########  ",
        "   ##  ##   ",
        "   ##  ##   ",
        "    ####    ",
        "   ######   ",
        "   ##  ##   ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 3 JUMP - tucked, the rigid arcade arc
    ("ART_JUMP", [
        "            ",
        "            ",
        "     ++     ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "   ######   ",
        " #####++### ",
        " #####++### ",
        "  ########  ",
        "  ###  ###  ",
        " ###    ### ",
        " ##      ## ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 4 THROW - arm extended, lance released
    ("ART_THROW", [
        "            ",
        "            ",
        "     ++     ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "   ######+++",
        "  ###++###  ",
        "  ###++###  ",
        "  ########  ",
        "   ##  ##   ",
        "   ##  ##   ",
        "   ##  ##   ",
        "  ###  ###  ",
        "  ###  ###  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 5 LANCE - the thrown weapon
    ("LANCE", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "  +#########",
        "  +#########",
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
    # 6 ZOMBIE rising - half out of the soil
    ("ZOM_RISE", [
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
        "    ####    ",
        "    #..#    ",
        "   ######   ",
        "  ###..###  ",
        "  ########  ",
        "  ########  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 7 ZOMBIE walk A - arms out
    ("ZOM_WALK1", [
        "            ",
        "            ",
        "            ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "  +######+  ",
        "  +######+  ",
        "   ######   ",
        "   ##  ##   ",
        "   ##  ##   ",
        "   ##  ##   ",
        "  ###  ###  ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 8 ZOMBIE walk B - shambling step
    ("ZOM_WALK2", [
        "            ",
        "            ",
        "            ",
        "    ####    ",
        "    #..#    ",
        "    ####    ",
        "  ++#####   ",
        "   ######+  ",
        "   ######   ",
        "   ##  ##   ",
        "  ##    ##  ",
        "  ##    ##  ",
        " ###    ### ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
    ]),
    # 9 CROW - the graveyard bird, wings down
    ("CROW1", [
        "            ",
        "            ",
        "            ",
        "            ",
        "            ",
        "  ##    ##  ",
        "   ##  ##   ",
        "    ####+   ",
        "    ####    ",
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
        "            ",
        "            ",
    ]),
    # 10 CROW - wings up
    ("CROW2", [
        "            ",
        "            ",
        "  ##        ",
        "   ##   ##  ",
        "    ## ##   ",
        "    #####   ",
        "    ####+   ",
        "     ###    ",
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
        "            ",
        "            ",
        "            ",
    ]),
    # 11 PUFF - the little cloud a kill leaves behind
    ("PUFF", [
        "            ",
        "            ",
        "            ",
        "            ",
        "   .    .   ",
        "  . ++++ .  ",
        "   ++++++   ",
        "  .++++++.  ",
        "   ++++++   ",
        "  . ++++ .  ",
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
]

# ---------------------------------------------------------------- level 1
# The graveyard: flat turf, a walled plateau in the middle, headstones
# and dead trees. Ground is given as the playfield row of the turf top;
# larger numbers are lower down the screen.

LEVEL_LEN = 208
GROUND_LOW = 18          # normal graveyard floor
GROUND_HIGH = 14         # the plateau

D_NONE, D_TOMB, D_CROSS, D_TREE, D_FENCE = 0, 1, 2, 3, 4


def build_level():
    ground = [GROUND_LOW] * LEVEL_LEN
    deco = [D_NONE] * LEVEL_LEN

    # a plateau you have to jump onto, with a ramp of two steps
    for c in range(70, 76):
        ground[c] = 16
    for c in range(76, 130):
        ground[c] = GROUND_HIGH
    for c in range(130, 136):
        ground[c] = 16

    # headstones, crosses and dead trees along the way
    for c in (8, 20, 34, 47, 58, 96, 110, 148, 162, 184, 198):
        deco[c] = D_TOMB
    for c in (14, 41, 64, 103, 141, 176):
        deco[c] = D_CROSS
    for c in (26, 53, 88, 120, 156, 191):
        deco[c] = D_TREE
    for c in (30, 31, 32, 168, 169, 170):
        deco[c] = D_FENCE
    return ground, deco


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
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        f.write("#define TILE_BASE 128\n")
        for i, (name, _) in enumerate(TILES):
            f.write(f"#define T_{name} {128 + i}\n")
        f.write(f"const unsigned char tile_gfx[{len(TILES) * 8}] = {{\n")
        for name, rows in TILES:
            f.write("    " + ", ".join(f"0x{b:02X}" for b in pack_mc_tile(rows))
                    + f", /* {name} */\n")
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


def emit_level(path):
    ground, deco = build_level()
    with open(path, "w") as f:
        f.write("/* generated by assetgen.py - do not edit */\n")
        f.write(f"#define LEVEL_LEN {LEVEL_LEN}\n")
        f.write(f"#define D_NONE {D_NONE}\n#define D_TOMB {D_TOMB}\n")
        f.write(f"#define D_CROSS {D_CROSS}\n#define D_TREE {D_TREE}\n")
        f.write(f"#define D_FENCE {D_FENCE}\n")
        for name, arr in (("level_ground", ground), ("level_deco", deco)):
            f.write(f"const unsigned char {name}[LEVEL_LEN] = {{\n")
            for o in range(0, LEVEL_LEN, 16):
                f.write("    " + ", ".join(str(v) for v in arr[o:o+16]) + ",\n")
            f.write("};\n")


if __name__ == "__main__":
    emit_tiles("tiles_mc.h")
    emit_sprites("sprites_mc.h")
    emit_level("level.h")
    print(f"tiles: {len(TILES)}  sprites: {len(SPRITES)}  "
          f"level: {LEVEL_LEN} columns")
