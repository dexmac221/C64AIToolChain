#!/usr/bin/env python3
"""Generate a C64 hi-res character deck from a bitmap source image.

The source is deck_bitmap.pgm, a plain PGM grayscale bitmap. The generator
splits it into 8x8 tiles, deduplicates those tiles into a custom C64 charset,
and writes screen/color tables for the fast character scroller.
"""

from pathlib import Path

BLACK = 0
WHITE = 1
CYAN = 3
BLUE = 6
YELLOW = 7
ORANGE = 8
LTRED = 10
GREY1 = 11
GREY2 = 12
LTBLUE = 14

TILE_W = 8
TILE_H = 8
BG_COLS = 96
BG_ROWS = 19
IMG_W = BG_COLS * TILE_W
IMG_H = BG_ROWS * TILE_H
TILE_BASE = 128
MAX_TILE_CHARS = 96
BORDER_CHAR = 126
BITMAP_PATH = Path(__file__).with_name("deck_bitmap.pgm")

FONT = {
    "A": ["  X  ", " X X ", "X   X", "X   X", "XXXXX", "X   X", "X   X"],
    "B": ["XXXX ", "X   X", "X   X", "XXXX ", "X   X", "X   X", "XXXX "],
    "C": [" XXXX", "X    ", "X    ", "X    ", "X    ", "X    ", " XXXX"],
    "D": ["XXXX ", "X   X", "X   X", "X   X", "X   X", "X   X", "XXXX "],
    "E": ["XXXXX", "X    ", "X    ", "XXXX ", "X    ", "X    ", "XXXXX"],
    "F": ["XXXXX", "X    ", "X    ", "XXXX ", "X    ", "X    ", "X    "],
    "G": [" XXXX", "X    ", "X    ", "X XXX", "X   X", "X   X", " XXXX"],
    "H": ["X   X", "X   X", "X   X", "XXXXX", "X   X", "X   X", "X   X"],
    "I": ["XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "XXXXX"],
    "J": ["XXXXX", "    X", "    X", "    X", "    X", "X   X", " XXX "],
    "K": ["X   X", "X  X ", "X X  ", "XX   ", "X X  ", "X  X ", "X   X"],
    "L": ["X    ", "X    ", "X    ", "X    ", "X    ", "X    ", "XXXXX"],
    "M": ["X   X", "XX XX", "X X X", "X   X", "X   X", "X   X", "X   X"],
    "N": ["X   X", "XX  X", "X X X", "X  XX", "X   X", "X   X", "X   X"],
    "O": [" XXX ", "X   X", "X   X", "X   X", "X   X", "X   X", " XXX "],
    "P": ["XXXX ", "X   X", "X   X", "XXXX ", "X    ", "X    ", "X    "],
    "Q": [" XXX ", "X   X", "X   X", "X   X", "X X X", "X  X ", " XX X"],
    "R": ["XXXX ", "X   X", "X   X", "XXXX ", "X X  ", "X  X ", "X   X"],
    "S": [" XXXX", "X    ", "X    ", " XXX ", "    X", "    X", "XXXX "],
    "T": ["XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  "],
    "U": ["X   X", "X   X", "X   X", "X   X", "X   X", "X   X", " XXX "],
    "V": ["X   X", "X   X", "X   X", "X   X", "X   X", " X X ", "  X  "],
    "W": ["X   X", "X   X", "X   X", "X   X", "X X X", "XX XX", "X   X"],
    "X": ["X   X", "X   X", " X X ", "  X  ", " X X ", "X   X", "X   X"],
    "Y": ["X   X", "X   X", " X X ", "  X  ", "  X  ", "  X  ", "  X  "],
    "Z": ["XXXXX", "    X", "   X ", "  X  ", " X   ", "X    ", "XXXXX"],
    "0": [" XXX ", "X   X", "X  XX", "X X X", "XX  X", "X   X", " XXX "],
    "1": ["  X  ", " XX  ", "  X  ", "  X  ", "  X  ", "  X  ", " XXX "],
    "2": [" XXX ", "X   X", "    X", "   X ", "  X  ", " X   ", "XXXXX"],
    "3": ["XXXX ", "    X", "    X", " XXX ", "    X", "    X", "XXXX "],
    "4": ["   X ", "  XX ", " X X ", "X  X ", "XXXXX", "   X ", "   X "],
    "5": ["XXXXX", "X    ", "X    ", "XXXX ", "    X", "    X", "XXXX "],
    "6": [" XXX ", "X    ", "X    ", "XXXX ", "X   X", "X   X", " XXX "],
    "7": ["XXXXX", "    X", "   X ", "  X  ", " X   ", " X   ", " X   "],
    "8": [" XXX ", "X   X", "X   X", " XXX ", "X   X", "X   X", " XXX "],
    "9": [" XXX ", "X   X", "X   X", " XXXX", "    X", "    X", " XXX "],
    "=": ["     ", "XXXXX", "     ", "     ", "XXXXX", "     ", "     "],
    " ": ["     ", "     ", "     ", "     ", "     ", "     ", "     "],
}


def new_bitmap() -> list[list[int]]:
    return [[0 for _ in range(IMG_W)] for _ in range(IMG_H)]


def rect(pixels: list[list[int]], x: int, y: int, w: int, h: int, value: int) -> None:
    for py in range(max(0, y), min(IMG_H, y + h)):
        for px in range(max(0, x), min(IMG_W, x + w)):
            pixels[py][px] = value


def line(pixels: list[list[int]], x0: int, y0: int, x1: int, y1: int, value: int) -> None:
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    x = x0
    y = y0
    while True:
        if 0 <= x < IMG_W and 0 <= y < IMG_H:
            pixels[y][x] = value
        if x == x1 and y == y1:
            break
        err2 = err * 2
        if err2 >= dy:
            err += dy
            x += sx
        if err2 <= dx:
            err += dx
            y += sy


def draw_default_bitmap() -> None:
    pixels = new_bitmap()
    for x in range(0, IMG_W, 24):
        rect(pixels, x + 4, 10, 4, 4, 1)
        rect(pixels, x + 12, 132, 3, 3, 1)

    for y in (24, 27, 123, 126):
        for x in range(0, IMG_W, 16):
            rect(pixels, x, y, 10, 2, 1)

    for x in range(0, IMG_W, 128):
        rect(pixels, x + 6, 42, 78, 24, 2)
        rect(pixels, x + 14, 48, 22, 9, 3)
        rect(pixels, x + 48, 50, 28, 5, 1)
        rect(pixels, x + 82, 38, 18, 34, 2)
        rect(pixels, x + 88, 44, 8, 9, 1)
        rect(pixels, x + 116, 88, 88, 18, 2)
        rect(pixels, x + 128, 94, 36, 8, 3)
        rect(pixels, x + 176, 94, 20, 5, 1)
        rect(pixels, x + 224, 36, 78, 22, 2)
        rect(pixels, x + 236, 42, 24, 9, 3)
        rect(pixels, x + 270, 42, 24, 5, 1)
        rect(pixels, x + 314, 80, 64, 20, 2)
        rect(pixels, x + 326, 86, 18, 8, 3)
        rect(pixels, x + 360, 86, 12, 5, 1)

        line(pixels, x + 52, 70, x + 86, 104, 1)
        line(pixels, x + 58, 70, x + 92, 104, 1)
        line(pixels, x + 150, 34, x + 188, 16, 3)
        line(pixels, x + 188, 16, x + 226, 34, 3)
        line(pixels, x + 156, 39, x + 188, 24, 2)
        line(pixels, x + 188, 24, x + 220, 39, 2)
        line(pixels, x + 244, 72, x + 280, 108, 1)
        line(pixels, x + 280, 72, x + 244, 108, 1)

    for x in range(0, IMG_W, 32):
        rect(pixels, x + 4, 14, 2, 2, 3)
        rect(pixels, x + 18, 116, 2, 2, 3)
        rect(pixels, x + 28, 140, 2, 2, 3)

    write_pgm(BITMAP_PATH, pixels)


def write_pgm(path: Path, pixels: list[list[int]]) -> None:
    lines = ["P2", f"# Dreadline hi-res character deck source, {IMG_W}x{IMG_H}", f"{IMG_W} {IMG_H}", "3"]
    for row in pixels:
        lines.append(" ".join(str(value) for value in row))
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def read_pgm(path: Path) -> list[list[int]] | None:
    tokens = []
    for line_text in path.read_text(encoding="ascii").splitlines():
        line_text = line_text.split("#", 1)[0]
        tokens.extend(line_text.split())
    if not tokens or tokens[0] != "P2":
        raise ValueError(f"{path} must be a plain PGM P2 bitmap")
    width = int(tokens[1])
    height = int(tokens[2])
    maxval = int(tokens[3])
    values = [int(token) for token in tokens[4:]]
    if width != IMG_W or height != IMG_H:
        return None
    if maxval < 3:
        raise ValueError(f"{path} max value must be at least 3")
    if len(values) != width * height:
        raise ValueError(f"{path} pixel count mismatch")
    return [values[index : index + width] for index in range(0, len(values), width)]


def tile_bytes(tile: tuple[int, ...]) -> tuple[int, ...]:
    rows = []
    for y in range(TILE_H):
        value = 0
        for x in range(TILE_W):
            value = (value << 1) | (1 if tile[y * TILE_W + x] else 0)
        rows.append(value)
    return tuple(rows)


def color_for_tile(tile: tuple[int, ...]) -> int:
    counts = [0, 0, 0, 0]
    for value in tile:
        counts[min(3, value)] += 1
    lit = counts[1] + counts[2] + counts[3]
    if lit == 0:
        return BLACK
    dominant = max(range(1, 4), key=lambda index: counts[index])
    if dominant == 3:
        return CYAN if lit > 10 else YELLOW
    if dominant == 2:
        return WHITE if lit > 24 else GREY2
    return LTBLUE if lit > 12 else BLUE


def glyph_from_font(char: str) -> tuple[int, ...]:
    rows = FONT.get(char, FONT[" "])
    out = []
    for source_y in range(8):
        value = 0
        for x in range(8):
            lit = source_y > 0 and x > 0 and x < 6 and rows[source_y - 1][x - 1] != " "
            value = (value << 1) | (1 if lit else 0)
        out.append(value)
    return tuple(out)


def base_charset() -> list[tuple[int, ...]]:
    charset = [(0,) * 8 for _ in range(256)]
    for index in range(26):
        charset[index + 1] = glyph_from_font(chr(ord("A") + index))
    for char in "0123456789= ":
        charset[ord(char)] = glyph_from_font(char)
    charset[BORDER_CHAR] = (0xFF,) * 8
    return charset


def convert_bitmap(pixels: list[list[int]]) -> tuple[list[list[int]], list[list[int]], list[tuple[int, ...]], int]:
    charset = base_charset()
    tile_to_code: dict[tuple[int, ...], int] = {}
    screen = []
    color = []
    next_code = TILE_BASE

    for cy in range(BG_ROWS):
        screen_row = []
        color_row = []
        for cx in range(BG_COLS):
            tile = tuple(
                min(3, pixels[cy * TILE_H + py][cx * TILE_W + px])
                for py in range(TILE_H)
                for px in range(TILE_W)
            )
            if max(tile) == 0:
                code = 32
            else:
                bitmap = tile_bytes(tile)
                if bitmap not in tile_to_code:
                    if next_code >= TILE_BASE + MAX_TILE_CHARS:
                        raise ValueError("too many unique bitmap tiles; simplify deck_bitmap.pgm")
                    tile_to_code[bitmap] = next_code
                    charset[next_code] = bitmap
                    next_code += 1
                code = tile_to_code[bitmap]
            screen_row.append(code)
            color_row.append(color_for_tile(tile))
        screen.append(screen_row)
        color.append(color_row)
    return screen, color, charset, next_code - TILE_BASE


def format_matrix(name: str, rows: list[list[int]]) -> str:
    lines = [f"static const unsigned char {name}[DREADLINE_BG_ROWS][DREADLINE_BG_WIDTH] = {{"]
    for row in rows:
        lines.append("    {")
        for index in range(0, BG_COLS, 16):
            chunk = row[index : index + 16]
            lines.append("        " + ",".join(f"0x{value:02X}" for value in chunk) + ",")
        lines.append("    },")
    lines.append("};")
    return "\n".join(lines)


def format_charset(charset: list[tuple[int, ...]]) -> str:
    lines = ["static const unsigned char dreadline_charset[2048] = {"]
    flat = [byte for glyph in charset for byte in glyph]
    for index in range(0, len(flat), 16):
        chunk = flat[index : index + 16]
        lines.append("    " + ",".join(f"0x{value:02X}" for value in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    pixels = read_pgm(BITMAP_PATH) if BITMAP_PATH.exists() else None
    if pixels is None:
        draw_default_bitmap()
        print(f"wrote {BITMAP_PATH}")
        pixels = read_pgm(BITMAP_PATH)
        if pixels is None:
            raise RuntimeError("failed to create bitmap source")

    screen, color, charset, tile_count = convert_bitmap(pixels)
    output = Path(__file__).with_name("background_mc.h")
    output.write_text(
        "#ifndef DREADLINE_BACKGROUND_MC_H\n"
        "#define DREADLINE_BACKGROUND_MC_H\n\n"
        "/* Generated by bggen.py from deck_bitmap.pgm. Edit the bitmap, then rebuild. */\n\n"
        f"#define DREADLINE_BG_WIDTH {BG_COLS}\n"
        f"#define DREADLINE_BG_ROWS {BG_ROWS}\n"
        f"#define DREADLINE_BORDER_CHAR {BORDER_CHAR}\n"
        f"#define DREADLINE_TILE_COUNT {tile_count}\n\n"
        + format_charset(charset)
        + "\n\n"
        + format_matrix("dreadline_bg_screen", screen)
        + "\n\n"
        + format_matrix("dreadline_bg_color", color)
        + "\n\n#endif\n",
        encoding="ascii",
    )
    print(f"wrote {output} ({tile_count} custom deck chars)")


if __name__ == "__main__":
    main()
