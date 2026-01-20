/*
 * Scroller Demo for C64 - Written in C using cc65
 * Smooth scrolling text message with sine wave
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
#define SCROLL_ROW 12

// Scroll message
static const char message[] =
    "     WELCOME TO THE C64 SCROLLER DEMO!     "
    "THIS TEXT SCROLLS SMOOTHLY FROM RIGHT TO LEFT...     "
    "THE COMMODORE 64 WAS RELEASED IN 1982 AND BECAME "
    "THE BEST-SELLING COMPUTER OF ALL TIME!     "
    "CREATED WITH CC65 AND THE AI TOOLCHAIN PROJECT...     "
    "GREETINGS TO ALL RETRO COMPUTING FANS!     "
    "                    ";

// Sine table for wave effect (32 values, amplitude 4)
static const signed char sinetab[32] = {
    0, 1, 2, 2, 3, 3, 4, 4,
    4, 4, 3, 3, 2, 2, 1, 0,
    0, -1, -2, -2, -3, -3, -4, -4,
    -4, -4, -3, -3, -2, -2, -1, 0
};

// Color cycle for rainbow effect
static const unsigned char rainbow[8] = {
    2,   // red
    8,   // orange
    7,   // yellow
    5,   // green
    3,   // cyan
    14,  // light blue
    6,   // blue
    4    // purple
};

static unsigned int scroll_pos;
static unsigned char wave_offset;
static unsigned char color_offset;
static unsigned int msg_len;

void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

// Convert PETSCII to screen code
unsigned char to_screencode(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 1;
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 1;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 48;
    }
    if (c == ' ') return 32;
    if (c == '!') return 33;
    if (c == '.') return 46;
    if (c == ',') return 44;
    if (c == '-') return 45;
    if (c == '\'') return 39;
    return 32;  // Default to space
}

// Draw scrolling text with wave effect
void draw_scroll(void) {
    unsigned char x;
    unsigned int pos;
    unsigned char ch;
    signed char wave;
    unsigned char row;
    unsigned char color;
    unsigned int text_idx;

    // Clear scroll area (rows 8-16)
    for (row = 8; row <= 16; row++) {
        pos = row * SCR_W;
        for (x = 0; x < SCR_W; x++) {
            SCREEN[pos + x] = 32;
        }
    }

    // Draw each character with wave offset
    for (x = 0; x < SCR_W; x++) {
        // Get character from message
        text_idx = (scroll_pos + x) % msg_len;
        ch = to_screencode(message[text_idx]);

        // Calculate wave offset for this column
        wave = sinetab[(x + wave_offset) & 31];
        row = SCROLL_ROW + wave;

        // Clamp to safe range
        if (row < 8) row = 8;
        if (row > 16) row = 16;

        // Draw character
        pos = row * SCR_W + x;
        SCREEN[pos] = ch;

        // Rainbow color
        color = rainbow[(x + color_offset) & 7];
        COLORS[pos] = color;
    }
}

// Draw decorative border
void draw_border(void) {
    unsigned char x;
    unsigned int pos;

    // Top border (row 6)
    pos = 6 * SCR_W;
    for (x = 0; x < SCR_W; x++) {
        SCREEN[pos + x] = 64;  // Horizontal line
        COLORS[pos + x] = BLUE;
    }

    // Bottom border (row 18)
    pos = 18 * SCR_W;
    for (x = 0; x < SCR_W; x++) {
        SCREEN[pos + x] = 64;
        COLORS[pos + x] = BLUE;
    }
}

int main(void) {
    unsigned char frame = 0;

    // Title
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    gotoxy(13, 10);
    textcolor(WHITE);
    cprintf("S C R O L L E R");

    gotoxy(10, 13);
    textcolor(CYAN);
    cprintf("SINE WAVE TEXT DEMO");

    gotoxy(11, 16);
    textcolor(GREY3);
    cprintf("PRESS ANY KEY...");

    cgetc();

    // Init
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    msg_len = strlen(message);
    scroll_pos = 0;
    wave_offset = 0;
    color_offset = 0;

    // Draw title
    gotoxy(13, 2);
    textcolor(YELLOW);
    cprintf("SINE SCROLLER");

    // Draw borders
    draw_border();

    // Main loop
    while (1) {
        wait_vblank();
        frame++;

        // Update scroll position (move left)
        if ((frame & 1) == 0) {
            scroll_pos++;
            if (scroll_pos >= msg_len) {
                scroll_pos = 0;
            }
        }

        // Update wave animation
        wave_offset++;

        // Update color cycle (slower)
        if ((frame & 3) == 0) {
            color_offset++;
        }

        // Draw scrolling text
        draw_scroll();
    }

    return 0;
}
