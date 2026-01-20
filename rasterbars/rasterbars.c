/*
 * Raster Bars Demo for C64 - Written in C using cc65
 * Colorful horizontal bars moving on screen
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

// VIC rasterline register
#define RASTERLINE (*(unsigned char*)0xD012)

// Screen dimensions
#define SCR_W 40
#define SCR_H 25

// Number of raster bars
#define NUM_BARS 6

// Bar colors (gradients for each bar)
static const unsigned char bar_colors[NUM_BARS][5] = {
    {0, 9, 2, 10, 2},   // Red gradient
    {0, 9, 8, 7, 8},    // Orange/yellow gradient
    {0, 11, 5, 13, 5},  // Green gradient
    {0, 11, 6, 14, 6},  // Blue gradient
    {0, 11, 4, 10, 4},  // Purple gradient
    {0, 11, 3, 1, 3}    // Cyan/white gradient
};

// Sine table for bar movement (32 values, bigger amplitude)
static const unsigned char sinetab[32] = {
    12, 14, 16, 17, 19, 20, 21, 21,
    22, 21, 21, 20, 19, 17, 16, 14,
    12, 10,  8,  7,  5,  4,  3,  3,
     2,  3,  3,  4,  5,  7,  8, 10
};

// Bar positions (row on screen)
static unsigned char bar_pos[NUM_BARS];
static unsigned char bar_phase[NUM_BARS];

void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

// Initialize bar positions
void init_bars(void) {
    unsigned char i;

    for (i = 0; i < NUM_BARS; i++) {
        bar_phase[i] = i * 5;  // Stagger phases
        bar_pos[i] = sinetab[bar_phase[i] & 31];
    }
}

// Clear screen to black
void clear_screen(void) {
    unsigned int i;

    for (i = 0; i < 1000; i++) {
        SCREEN[i] = 160;  // Solid block
        COLORS[i] = BLACK;
    }
}

// Draw a single raster bar
void draw_bar(unsigned char bar_num, unsigned char row) {
    unsigned char x, y;
    unsigned int pos;
    unsigned char color;

    // Draw 5-line gradient bar
    for (y = 0; y < 5; y++) {
        if (row + y >= SCR_H) continue;

        pos = (row + y) * SCR_W;
        color = bar_colors[bar_num][y];

        for (x = 0; x < SCR_W; x++) {
            // Only draw if not overlapping darker bar
            if (COLORS[pos + x] < color || COLORS[pos + x] == BLACK) {
                COLORS[pos + x] = color;
            }
        }
    }
}

// Update and draw all bars
void update_bars(void) {
    unsigned char i;

    // Clear screen colors
    clear_screen();

    // Update positions and draw each bar
    for (i = 0; i < NUM_BARS; i++) {
        // Update phase (all same speed for smooth wave)
        bar_phase[i] += 1;

        // Get position from sine table
        bar_pos[i] = sinetab[bar_phase[i] & 31];

        // Draw the bar
        draw_bar(i, bar_pos[i]);
    }
}

// Draw title overlay
void draw_title(void) {
    gotoxy(12, 0);
    textcolor(WHITE);
    COLORS[12] = WHITE;  // Fix first char color
    cprintf("RASTER BARS");
}

int main(void) {
    unsigned char frame = 0;

    // Title screen
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    gotoxy(11, 10);
    textcolor(WHITE);
    cprintf("R A S T E R  B A R S");

    gotoxy(9, 13);
    textcolor(CYAN);
    cprintf("COLORFUL MOVING STRIPES");

    gotoxy(12, 16);
    textcolor(GREY3);
    cprintf("PRESS ANY KEY...");

    cgetc();

    // Init
    bgcolor(BLACK);
    bordercolor(BLACK);
    init_bars();
    clear_screen();

    // Fill screen with solid blocks
    {
        unsigned int i;
        for (i = 0; i < 1000; i++) {
            SCREEN[i] = 160;
        }
    }

    // Main loop
    while (1) {
        wait_vblank();
        frame++;

        // Update bars
        update_bars();

        // Draw title every few frames
        if ((frame & 15) == 0) {
            draw_title();
        }
    }

    return 0;
}
