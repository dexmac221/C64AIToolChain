/*
 * Bouncing Ball for C64 - Written in C using cc65
 * Simple ball bouncing off walls with sound and color effects
 */

#include <c64.h>
#include <conio.h>
#include <stdlib.h>

// Sprite pointers at end of screen RAM
#define SPRITE_PTRS ((unsigned char*)0x07F8)

// VIC-II registers
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)
#define VIC_SPR_DBL_X  (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y  (*(unsigned char*)0xD017)

// Sprite positions
#define SPR0_X (*(unsigned char*)0xD000)
#define SPR0_Y (*(unsigned char*)0xD001)

// Sprite color
#define SPR0_COL (*(unsigned char*)0xD027)

// SID registers for sound
#define SID_FREQ_LO    (*(unsigned char*)0xD400)
#define SID_FREQ_HI    (*(unsigned char*)0xD401)
#define SID_PW_LO      (*(unsigned char*)0xD402)
#define SID_PW_HI      (*(unsigned char*)0xD403)
#define SID_CTRL       (*(unsigned char*)0xD404)
#define SID_AD         (*(unsigned char*)0xD405)
#define SID_SR         (*(unsigned char*)0xD406)
#define SID_VOLUME     (*(unsigned char*)0xD418)

// Sprite data location - $3000 (block 192)
#define SPRITE_DATA 0x3000
#define SPRITE_BLOCK_BALL 192

// Screen boundaries (sprite coordinates)
#define WALL_TOP     50
#define WALL_BOTTOM  235
#define WALL_LEFT    24
#define WALL_RIGHT   255

// Colors
#define BLACK   0
#define WHITE   1
#define RED     2
#define CYAN    3
#define PURPLE  4
#define GREEN   5
#define BLUE    6
#define YELLOW  7
#define ORANGE  8
#define LTRED   10
#define LTGREEN 13
#define LTBLUE  14

// Ball colors to cycle through
static const unsigned char ball_colors[8] = {
    WHITE, YELLOW, CYAN, GREEN, LTGREEN, LTBLUE, PURPLE, ORANGE
};

// Game variables
static unsigned char ball_x;
static unsigned char ball_y;
static signed char ball_dx;
static signed char ball_dy;
static unsigned char color_index;
static unsigned char bounce_count;

// Ball sprite data - nice round ball
static const unsigned char ball_sprite[63] = {
    0x00, 0x00, 0x00,  // Row 0
    0x00, 0x00, 0x00,  // Row 1
    0x00, 0x00, 0x00,  // Row 2
    0x00, 0x7C, 0x00,  // Row 3:   .XXXXX.
    0x01, 0xFF, 0x00,  // Row 4:  .XXXXXXX.
    0x03, 0xFF, 0x80,  // Row 5: .XXXXXXXXX.
    0x07, 0xFF, 0xC0,  // Row 6: XXXXXXXXXXX
    0x07, 0xFF, 0xC0,  // Row 7: XXXXXXXXXXX
    0x0F, 0xFF, 0xE0,  // Row 8: XXXXXXXXXXXXX
    0x0F, 0xFF, 0xE0,  // Row 9: XXXXXXXXXXXXX
    0x0F, 0xFF, 0xE0,  // Row 10: XXXXXXXXXXXXX
    0x07, 0xFF, 0xC0,  // Row 11: XXXXXXXXXXX
    0x07, 0xFF, 0xC0,  // Row 12: XXXXXXXXXXX
    0x03, 0xFF, 0x80,  // Row 13: .XXXXXXXXX.
    0x01, 0xFF, 0x00,  // Row 14: .XXXXXXX.
    0x00, 0x7C, 0x00,  // Row 15:  .XXXXX.
    0x00, 0x00, 0x00,  // Row 16
    0x00, 0x00, 0x00,  // Row 17
    0x00, 0x00, 0x00,  // Row 18
    0x00, 0x00, 0x00,  // Row 19
    0x00, 0x00, 0x00   // Row 20
};

// Wait for vertical blank
void wait_vblank(void) {
    while (VIC.rasterline != 255) ;
}

// Initialize SID
void init_sound(void) {
    SID_VOLUME = 15;
    SID_AD = 0x00;
    SID_SR = 0xA0;
    SID_PW_LO = 0x00;
    SID_PW_HI = 0x08;
}

// Play bounce sound - pitch varies with position
void sound_bounce(unsigned char pitch) {
    SID_FREQ_LO = 0x00;
    SID_FREQ_HI = pitch;
    SID_CTRL = 0x11;  // Triangle wave
}

void sound_off(void) {
    SID_CTRL = 0x10;
}

// Initialize sprite
void init_sprite(void) {
    unsigned char* dest;
    unsigned char i;

    // Copy ball sprite data
    dest = (unsigned char*)SPRITE_DATA;
    for (i = 0; i < 63; i++) {
        dest[i] = ball_sprite[i];
    }

    // Set sprite pointer
    SPRITE_PTRS[0] = SPRITE_BLOCK_BALL;

    // Enable sprite 0
    VIC_SPR_ENA = 0x01;

    // Initial color
    SPR0_COL = WHITE;

    // Expand sprite for bigger ball
    VIC_SPR_DBL_X = 0x01;
    VIC_SPR_DBL_Y = 0x01;

    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X = 0;
}

// Initialize ball position and velocity
void init_ball(void) {
    ball_x = 140;
    ball_y = 140;

    // Random initial direction
    ball_dx = (rand() & 1) ? 2 : -2;
    ball_dy = (rand() & 1) ? 2 : -2;

    // Add some variation
    if (rand() & 1) ball_dx += (ball_dx > 0) ? 1 : -1;
    if (rand() & 1) ball_dy += (ball_dy > 0) ? 1 : -1;

    color_index = 0;
    bounce_count = 0;
}

// Update sprite position
void update_sprite(void) {
    SPR0_X = ball_x;
    SPR0_Y = ball_y;
}

// Move ball and handle collisions
void move_ball(void) {
    unsigned char new_x, new_y;
    unsigned char bounced = 0;
    unsigned char pitch = 0x20;

    new_x = ball_x + ball_dx;
    new_y = ball_y + ball_dy;

    // Left wall
    if (new_x <= WALL_LEFT || (ball_dx < 0 && new_x > ball_x)) {
        ball_dx = -ball_dx;
        new_x = WALL_LEFT + 1;
        bounced = 1;
        pitch = 0x30;
    }

    // Right wall
    if (new_x >= WALL_RIGHT - 24) {
        ball_dx = -ball_dx;
        new_x = WALL_RIGHT - 25;
        bounced = 1;
        pitch = 0x28;
    }

    // Top wall
    if (new_y <= WALL_TOP) {
        ball_dy = -ball_dy;
        new_y = WALL_TOP + 1;
        bounced = 1;
        pitch = 0x38;
    }

    // Bottom wall
    if (new_y >= WALL_BOTTOM - 24) {
        ball_dy = -ball_dy;
        new_y = WALL_BOTTOM - 25;
        bounced = 1;
        pitch = 0x20;
    }

    // On bounce, change color and play sound
    if (bounced) {
        bounce_count++;
        color_index = (color_index + 1) & 7;
        SPR0_COL = ball_colors[color_index];
        sound_bounce(pitch);

        // Occasionally change border color too
        if (bounce_count & 3) {
            bordercolor(ball_colors[color_index]);
        }
    }

    ball_x = new_x;
    ball_y = new_y;
}

// Draw status
void draw_status(void) {
    gotoxy(1, 0);
    textcolor(WHITE);
    cprintf("BOUNCING BALL");

    gotoxy(28, 0);
    textcolor(YELLOW);
    cprintf("BOUNCES:%u", bounce_count);
}

// Main
int main(void) {
    unsigned char frame = 0;

    // Setup
    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    init_sound();
    init_sprite();
    init_ball();

    // Title
    gotoxy(12, 12);
    textcolor(YELLOW);
    cprintf("BOUNCING BALL");
    gotoxy(10, 14);
    textcolor(WHITE);
    cprintf("PRESS ANY KEY...");

    // Wait for key
    cgetc();
    clrscr();

    // Main loop
    while (1) {
        wait_vblank();
        frame++;

        // Sound off after short time
        if ((frame & 7) == 0) {
            sound_off();
        }

        move_ball();
        update_sprite();

        // Update status occasionally
        if ((frame & 15) == 0) {
            draw_status();
        }
    }

    return 0;
}
