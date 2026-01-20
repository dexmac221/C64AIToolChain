/*
 * Starfield Demo for C64 - Written in C using cc65
 * 3D starfield flying through space effect
 */

#include <c64.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

// Screen pointers
#define SCREEN ((unsigned char*)0x0400)
#define COLORS ((unsigned char*)0xD800)

// Colors
#define BLACK   0
#define WHITE   1
#define LTGREY  15
#define GREY    12
#define DKGREY  11

// Star characters (different sizes for depth)
#define STAR_FAR    0x2E  // . (period - small/far)
#define STAR_MED    0x51  // * style
#define STAR_NEAR   0x58  // Larger star

// Number of stars per layer
#define STARS_FAR   25
#define STARS_MED   20
#define STARS_NEAR  15
#define TOTAL_STARS (STARS_FAR + STARS_MED + STARS_NEAR)

// Screen dimensions
#define SCR_W 40
#define SCR_H 25
#define CENTER_X 20
#define CENTER_Y 12

// Star data
static unsigned char star_x[TOTAL_STARS];
static unsigned char star_y[TOTAL_STARS];
static signed char star_dx[TOTAL_STARS];
static signed char star_dy[TOTAL_STARS];
static unsigned char star_layer[TOTAL_STARS];  // 0=far, 1=med, 2=near

// Wait for vertical blank
void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

// Clear a star from screen
void clear_star(unsigned char x, unsigned char y) {
    if (x < SCR_W && y < SCR_H) {
        SCREEN[y * SCR_W + x] = 0x20;  // Space
    }
}

// Draw a star on screen
void draw_star(unsigned char x, unsigned char y, unsigned char layer) {
    unsigned int pos;
    unsigned char chr, col;

    if (x >= SCR_W || y >= SCR_H) return;

    pos = y * SCR_W + x;

    switch (layer) {
        case 0:  // Far - small, dark
            chr = STAR_FAR;
            col = DKGREY;
            break;
        case 1:  // Medium
            chr = STAR_MED;
            col = GREY;
            break;
        case 2:  // Near - large, bright
            chr = STAR_NEAR;
            col = WHITE;
            break;
        default:
            chr = STAR_FAR;
            col = GREY;
    }

    SCREEN[pos] = chr;
    COLORS[pos] = col;
}

// Initialize a single star at center
void init_star(unsigned char i) {
    signed char dx, dy;

    // Start near center with small random offset
    star_x[i] = CENTER_X + (rand() % 5) - 2;
    star_y[i] = CENTER_Y + (rand() % 3) - 1;

    // Random direction from center
    do {
        dx = (rand() % 5) - 2;
        dy = (rand() % 5) - 2;
    } while (dx == 0 && dy == 0);  // Ensure movement

    star_dx[i] = dx;
    star_dy[i] = dy;

    // Assign layer based on index
    if (i < STARS_FAR) {
        star_layer[i] = 0;  // Far
    } else if (i < STARS_FAR + STARS_MED) {
        star_layer[i] = 1;  // Medium
    } else {
        star_layer[i] = 2;  // Near
    }
}

// Initialize all stars
void init_stars(void) {
    unsigned char i;

    for (i = 0; i < TOTAL_STARS; i++) {
        init_star(i);
        // Spread initial positions across screen
        star_x[i] = rand() % SCR_W;
        star_y[i] = rand() % SCR_H;
    }
}

// Move stars based on layer speed
void move_stars(void) {
    unsigned char i;
    unsigned char speed;
    signed int new_x, new_y;

    for (i = 0; i < TOTAL_STARS; i++) {
        // Clear old position
        clear_star(star_x[i], star_y[i]);

        // Speed based on layer (far=slow, near=fast)
        speed = star_layer[i] + 1;

        // Move star
        new_x = (signed int)star_x[i] + (star_dx[i] * speed) / 2;
        new_y = (signed int)star_y[i] + (star_dy[i] * speed) / 2;

        // Check if star left screen
        if (new_x < 0 || new_x >= SCR_W || new_y < 0 || new_y >= SCR_H) {
            // Respawn at center
            init_star(i);
        } else {
            star_x[i] = (unsigned char)new_x;
            star_y[i] = (unsigned char)new_y;
        }

        // Draw at new position
        draw_star(star_x[i], star_y[i], star_layer[i]);
    }
}

// Draw title
void draw_title(void) {
    gotoxy(14, 0);
    textcolor(WHITE);
    cprintf("STARFIELD");
}

// Main
int main(void) {
    unsigned char frame = 0;

    // Setup screen
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLACK);

    // Intro text
    gotoxy(12, 10);
    textcolor(WHITE);
    cprintf("S T A R F I E L D");

    gotoxy(10, 13);
    textcolor(LTGREY);
    cprintf("FLYING THROUGH SPACE");

    gotoxy(10, 16);
    textcolor(GREY);
    cprintf("PRESS ANY KEY...");

    // Wait for key
    cgetc();

    // Clear and init
    clrscr();
    init_stars();

    // Main loop
    while (1) {
        wait_vblank();
        frame++;

        // Move and draw stars
        move_stars();

        // Draw title occasionally (every 64 frames to reduce flicker)
        if ((frame & 63) == 0) {
            draw_title();
        }
    }

    return 0;
}
