#!/usr/bin/env python3
"""Generate C64 multicolor sprite byte tables from 12x21 ASCII art.

C64 multicolor sprites use 12 logical pixels per row. Each logical pixel is
2 hardware pixels wide and encoded as a two-bit value:
  space = 00 transparent
  .     = 01 shared multicolor 0 ($D025)
  #     = 10 sprite individual color ($D027-$D02E)
  +     = 11 shared multicolor 1 ($D026)
"""

from pathlib import Path

PIXELS = {
    " ": 0,
    ".": 1,
    "#": 2,
    "+": 3,
}

SPRITES = {
    "ship_sprite_0": [
        "            ",
        "            ",
        "            ",
        "      ..#   ",
        "    ..###   ",
        "  ..#####   ",
        "  .#######  ",
        "+.######### ",
        "++##..#####.",
        "+##....#####",
        "+++..++..###",
        "+##....#####",
        "++##..#####.",
        "+.######### ",
        "  .#######  ",
        "  ..#####   ",
        "    ..###   ",
        "      ..#   ",
        "            ",
        "            ",
        "            ",
    ],
    "ship_sprite_1": [
        "            ",
        "      ..#   ",
        "    ..###   ",
        "  ..#####   ",
        "  .#######  ",
        "+.######### ",
        "++.##.#####.",
        "+++....#####",
        "+++.++..####",
        "+...++...###",
        "+++.++..####",
        "+++....#####",
        "++.##.#####.",
        "+.######### ",
        "  .#######  ",
        "  ..#####   ",
        "    ..###   ",
        "      ..#   ",
        "            ",
        "            ",
        "            ",
    ],
    "drone_sprite_0": [
        "            ",
        "    ++++    ",
        "   +####+   ",
        "  +######+  ",
        " .##.++.##. ",
        ".##..++..##.",
        ".##.####.##.",
        "###.####.###",
        "##..++++..##",
        "###.####.###",
        ".##.####.##.",
        ".##..++..##.",
        " .##.++.##. ",
        "  +######+  ",
        "   +####+   ",
        "    ++++    ",
        "  ++    ++  ",
        " ++      ++ ",
        "  ++    ++  ",
        "            ",
        "            ",
    ],
    "drone_sprite_1": [
        "            ",
        "    .##.    ",
        "   +####+   ",
        "  +######+  ",
        " .##.++.##. ",
        ".##..++..##.",
        ".##.####.##.",
        "###.####.###",
        "##..++++..##",
        "###.####.###",
        ".##.####.##.",
        ".##..++..##.",
        " .##.++.##. ",
        "  +######+  ",
        "   +####+   ",
        "    .##.    ",
        "++        ++",
        "  ++    ++  ",
        "    ++++    ",
        "            ",
        "            ",
    ],
    "turret_sprite_0": [
        "     ##     ",
        "    ####    ",
        "   +####+   ",
        "  +######+  ",
        " .###++###. ",
        ".###.++.###.",
        "###..##..###",
        "##...##...##",
        "     ##     ",
        "     ##     ",
        "    ####    ",
        "   ######   ",
        "  .######.  ",
        " ..######.. ",
        ".##########.",
        "############",
        "############",
        ".##########.",
        " ..######.. ",
        "            ",
        "            ",
    ],
    "turret_sprite_1": [
        "    ####    ",
        "   ######   ",
        "  +######+  ",
        " +###++###+ ",
        ".###.++.###.",
        "###..##..###",
        "##...##...##",
        "     ##     ",
        "     ##     ",
        "     ##     ",
        "    ####    ",
        "   ######   ",
        "  .######.  ",
        " ..######.. ",
        ".##########.",
        "############",
        "############",
        ".##########.",
        " ..######.. ",
        "            ",
        "            ",
    ],
    "core_sprite_0": [
        "    ++++    ",
        "   ++##++   ",
        "  ++####++  ",
        " ++##..##++ ",
        "+###.++.###+",
        "+##..##..##+",
        "##..++++..##",
        "##.++##++.##",
        "##.++##++.##",
        "##..++++..##",
        "+##..##..##+",
        "+###.++.###+",
        " ++##..##++ ",
        "  ++####++  ",
        "   ++##++   ",
        "    ++++    ",
        "     ##     ",
        "    +##+    ",
        "   ++++++   ",
        "            ",
        "            ",
    ],
    "core_sprite_1": [
        "   ++++++   ",
        "  ++####++  ",
        " ++######++ ",
        "+###.++.###+",
        "+##..##..##+",
        "##..++++..##",
        "##.++##++.##",
        "##++####++##",
        "##++####++##",
        "##.++##++.##",
        "##..++++..##",
        "+##..##..##+",
        "+###.++.###+",
        " ++######++ ",
        "  ++####++  ",
        "   ++++++   ",
        "    +##+    ",
        "   ++##++   ",
        "  ++++++++  ",
        "            ",
        "            ",
    ],
}


def pack_row(row: str) -> list[int]:
    if len(row) != 12:
        raise ValueError(f"row must be 12 chars, got {len(row)}: {row!r}")
    packed = []
    for group in range(3):
        value = 0
        for char in row[group * 4 : group * 4 + 4]:
            value = (value << 2) | PIXELS[char]
        packed.append(value)
    return packed


def sprite_bytes(rows: list[str]) -> list[int]:
    if len(rows) != 21:
        raise ValueError(f"sprite must be 21 rows, got {len(rows)}")
    data = []
    for row in rows:
        data.extend(pack_row(row))
    return data


def format_array(name: str, data: list[int]) -> str:
    lines = [f"static const unsigned char {name}[63] = {{"]
    for index in range(0, 63, 9):
        chunk = data[index : index + 9]
        lines.append("    " + ",".join(f"0x{value:02X}" for value in chunk) + ("," if index < 54 else ""))
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    output = Path(__file__).with_name("sprites_mc.h")
    arrays = [format_array(name, sprite_bytes(rows)) for name, rows in SPRITES.items()]
    output.write_text(
        "#ifndef DREADLINE_SPRITES_MC_H\n"
        "#define DREADLINE_SPRITES_MC_H\n\n"
        "/* Generated by spritegen.py. Edit the ASCII art there, then rerun it. */\n\n"
        + "\n\n".join(arrays)
        + "\n\n#endif\n",
        encoding="ascii",
    )
    print(f"wrote {output}")


if __name__ == "__main__":
    main()
