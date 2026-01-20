/*
 * Plasma Demo for C64 - Written in C using cc65
 * Colorful psychedelic color cycling effect
 */

#include <c64.h>
#include <conio.h>
#include <string.h>

// C64 Colors
#define BLACK       0
#define WHITE       1
#define RED         2
#define CYAN        3
#define PURPLE      4
#define GREEN       5
#define BLUE        6
#define YELLOW      7
#define ORANGE      8
#define BROWN       9
#define LIGHTRED   10
#define GREY1      11
#define GREY2      12
#define LIGHTGREEN 13
#define LIGHTBLUE  14
#define GREY3      15

// Screen pointers
#define SCREEN ((unsigned char*)0x0400)
#define COLORS ((unsigned char*)0xD800)

// Screen dimensions
#define SCR_W 40
#define SCR_H 25

// Sine table for plasma effect (64 values)
static const unsigned char sinetab[64] = {
    8, 9, 10, 11, 12, 13, 14, 14,
    15, 15, 15, 15, 15, 14, 14, 13,
    12, 11, 10, 9, 8, 7, 6, 5,
    4, 3, 2, 2, 1, 1, 1, 1,
    1, 2, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 14,
    15, 15, 15, 15, 15, 14, 14, 13,
    12, 11, 10, 9, 8, 7, 6, 5
};

// Color cycle (C64 colors that look good together)
static const unsigned char colorcycle[16] = {
    0,  // black
    11, // dark grey
    12, // medium grey
    15, // light grey
    1,  // white
    13, // light green
    5,  // green
    3,  // cyan
    14, // light blue
    6,  // blue
    4,  // purple
    10, // light red
    2,  // red
    8,  // orange
    7,  // yellow
    1   // white
};

// Character to use for plasma (solid block)
#define PLASMA_CHAR 160

static unsigned char offset1, offset2, offset3;

void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

// Calculate plasma value for position
unsigned char plasma_value(unsigned char x, unsigned char y) {
    unsigned char v1, v2, v3, v4;

    v1 = sinetab[(x + offset1) & 63];
    v2 = sinetab[(y + offset2) & 63];
    v3 = sinetab[((x + y) + offset3) & 63];
    v4 = sinetab[((x - y + 32) + offset1) & 63];

    return (v1 + v2 + v3 + v4) >> 2;  // Average, 0-15
}

// Update plasma colors
void update_plasma(void) {
    unsigned char x, y;
    unsigned int pos;
    unsigned char color;

    pos = 0;
    for (y = 0; y < SCR_H; y++) {
        for (x = 0; x < SCR_W; x++) {
            color = plasma_value(x, y);
            COLORS[pos] = colorcycle[color & 15];
            pos++;
        }
    }

    // Animate offsets
    offset1 += 1;
    offset2 += 2;
    offset3 += 3;
}

// Fill screen with solid characters
void init_screen(void) {
    unsigned int i;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    for (i = 0; i < 1000; i++) {
        SCREEN[i] = PLASMA_CHAR;
    }
}

int main(void) {
    // Title
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    gotoxy(14, 10);
    textcolor(WHITE);
    cprintf("P L A S M A");

    gotoxy(10, 13);
    textcolor(LIGHTBLUE);
    cprintf("COLOR CYCLING EFFECT");

    gotoxy(10, 16);
    textcolor(GREY3);
    cprintf("PRESS ANY KEY...");

    cgetc();

    // Init
    init_screen();
    offset1 = 0;
    offset2 = 0;
    offset3 = 0;

    // Main loop
    while (1) {
        wait_vblank();
        update_plasma();
    }

    return 0;
}
