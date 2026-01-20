/*
 * Fire Demo for C64 - Optimized version
 * Uses bit shifts instead of division for speed
 */

#include <c64.h>
#include <conio.h>
#include <stdlib.h>

// Screen pointers
#define SCREEN ((unsigned char*)0x0400)
#define COLORS ((unsigned char*)0xD800)

// Smaller fire area for speed (40x20)
#define FIRE_W 40
#define FIRE_H 20
#define FIRE_START_ROW 5

// Fire buffer
static unsigned char fire[FIRE_H + 1][FIRE_W];

// Fire colors
static const unsigned char firecolors[8] = {
    0,   // black
    9,   // brown
    2,   // red
    10,  // light red
    8,   // orange
    7,   // yellow
    7,   // yellow
    1    // white
};

#define FIRE_CHAR 160

// Fast pseudo-random (linear feedback)
static unsigned char rnd_seed = 0x42;
static unsigned char fast_rand(void) {
    rnd_seed = (rnd_seed >> 1) ^ (-(rnd_seed & 1) & 0xB8);
    return rnd_seed;
}

void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

// Generate heat - inline for speed
void generate_heat(void) {
    unsigned char x;
    unsigned char *row = fire[FIRE_H];

    for (x = 0; x < FIRE_W; x++) {
        row[x] = 5 + (fast_rand() & 3);
    }
}

// Optimized propagate - uses shifts instead of division
void propagate_fire(void) {
    unsigned char x, y;
    unsigned char *curr, *below;
    unsigned char sum;

    for (y = 0; y < FIRE_H; y++) {
        curr = fire[y];
        below = fire[y + 1];

        // First column
        sum = below[0] + below[0] + below[1];
        curr[0] = (sum >> 2) + (sum >> 4);  // Approximate /3
        if (curr[0] > 0 && (fast_rand() & 7) == 0) curr[0]--;

        // Middle columns - unrolled inner calculation
        for (x = 1; x < FIRE_W - 1; x++) {
            sum = below[x-1] + below[x] + below[x] + below[x+1];
            curr[x] = sum >> 2;  // Fast divide by 4
            if (curr[x] > 0 && (fast_rand() & 15) == 0) curr[x]--;
        }

        // Last column
        sum = below[FIRE_W-2] + below[FIRE_W-1] + below[FIRE_W-1];
        curr[FIRE_W-1] = (sum >> 2) + (sum >> 4);
        if (curr[FIRE_W-1] > 0 && (fast_rand() & 7) == 0) curr[FIRE_W-1]--;
    }
}

// Optimized render using pointers
void render_fire(void) {
    unsigned char x, y, val;
    unsigned char *scr = SCREEN + (FIRE_START_ROW * 40);
    unsigned char *clr = COLORS + (FIRE_START_ROW * 40);
    unsigned char *src;

    for (y = 0; y < FIRE_H; y++) {
        src = fire[y];
        for (x = 0; x < FIRE_W; x++) {
            val = src[x];
            if (val > 7) val = 7;
            scr[x] = FIRE_CHAR;
            clr[x] = firecolors[val];
        }
        scr += 40;
        clr += 40;
    }
}

int main(void) {
    unsigned int i;

    // Title
    clrscr();
    bgcolor(0);
    bordercolor(0);

    gotoxy(16, 10);
    textcolor(8);
    cputs("F I R E");

    gotoxy(12, 13);
    textcolor(7);
    cputs("FLAME EFFECT DEMO");

    gotoxy(11, 16);
    textcolor(15);
    cputs("PRESS ANY KEY...");

    cgetc();

    // Fill screen with solid blocks
    clrscr();
    bgcolor(0);
    bordercolor(0);

    for (i = 0; i < 1000; i++) {
        SCREEN[i] = FIRE_CHAR;
        COLORS[i] = 0;
    }

    // Init fire buffer to zero
    for (i = 0; i < (FIRE_H + 1) * FIRE_W; i++) {
        ((unsigned char*)fire)[i] = 0;
    }

    // Main loop - no vblank wait for max speed
    while (1) {
        generate_heat();
        propagate_fire();
        render_fire();
    }

    return 0;
}
