/*
 * Breakout for C64 - Written in C using cc65
 * Fixed play area with visible walls
 */

#include <c64.h>
#include <conio.h>
#include <stdlib.h>
#include <joystick.h>

// Sprite pointers
#define SPRITE_PTRS ((unsigned char*)0x07F8)

// VIC-II registers
#define VIC_SPR_ENA    (*(unsigned char*)0xD015)
#define VIC_SPR_HI_X   (*(unsigned char*)0xD010)
#define VIC_SPR_DBL_X  (*(unsigned char*)0xD01D)
#define VIC_SPR_DBL_Y  (*(unsigned char*)0xD017)
#define VIC_SPR_MCOLOR (*(unsigned char*)0xD01C)

// Sprite positions and colors
#define SPR0_X   (*(unsigned char*)0xD000)
#define SPR0_Y   (*(unsigned char*)0xD001)
#define SPR1_X   (*(unsigned char*)0xD002)
#define SPR1_Y   (*(unsigned char*)0xD003)
#define SPR0_COL (*(unsigned char*)0xD027)
#define SPR1_COL (*(unsigned char*)0xD028)

// SID registers
#define SID_FREQ_HI (*(unsigned char*)0xD401)
#define SID_CTRL    (*(unsigned char*)0xD404)
#define SID_AD      (*(unsigned char*)0xD405)
#define SID_SR      (*(unsigned char*)0xD406)
#define SID_VOLUME  (*(unsigned char*)0xD418)

// Sprite memory at $3000
#define SPRITE_DATA 0x3000

// Play area: columns 1-28 (leaving col 0 and 29+ for walls/score)
// Sprite X = 24 + column*8
// Column 1 = X 32, Column 28 = X 248
#define PLAY_LEFT_COL   1
#define PLAY_RIGHT_COL  28
#define WALL_LEFT       (24 + PLAY_LEFT_COL * 8)      // 32
#define WALL_RIGHT      (24 + (PLAY_RIGHT_COL+1) * 8) // 256, but we use 248
#define WALL_TOP        58
#define PADDLE_Y        216

// Brick area - 7 columns of bricks, 4 chars wide each = 28 chars
#define BRICK_ROWS      5
#define BRICK_COLS      7
#define BRICK_WIDTH     4
#define BRICK_START_COL 1
#define BRICK_START_Y   3

// Colors
#define BLACK   0
#define WHITE   1
#define RED     2
#define CYAN    3
#define GREEN   5
#define BLUE    6
#define YELLOW  7
#define ORANGE  8
#define LTBLUE  14
#define GREY    12

// Game state
static unsigned char paddle_x;
static unsigned char ball_x, ball_y;
static signed char ball_dx, ball_dy;
static unsigned int score;
static unsigned char lives;
static unsigned char bricks[BRICK_ROWS][BRICK_COLS];
static unsigned char bricks_left;
static unsigned char serve_mode;
static unsigned char level;

// Brick colors per row
static const unsigned char row_colors[5] = { RED, ORANGE, YELLOW, GREEN, CYAN };

// Paddle sprite - horizontal bar (18 pixels wide)
static const unsigned char paddle_data[63] = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x3F,0xFF,0x80,  // ############
    0x7F,0xFF,0xC0,  // ##############
    0x7F,0xFF,0xC0,  // ##############
    0x3F,0xFF,0x80,  // ############
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};

// Ball sprite - small circle
static const unsigned char ball_data[63] = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x78,0x00,  //    ####
    0x00,0xFC,0x00,  //   ######
    0x01,0xFE,0x00,  //  ########
    0x01,0xFE,0x00,  //  ########
    0x00,0xFC,0x00,  //   ######
    0x00,0x78,0x00,  //    ####
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00
};

#define PADDLE_WIDTH 18
#define BALL_SIZE    8

void wait_vblank(void) {
    while (VIC.rasterline != 255);
}

void init_sound(void) {
    SID_VOLUME = 15;
    SID_AD = 0x00;
    SID_SR = 0xA0;
}

void sound_bounce(unsigned char pitch) {
    SID_FREQ_HI = pitch;
    SID_CTRL = 0x11;
}

void sound_off(void) {
    SID_CTRL = 0x10;
}

void init_sprites(void) {
    unsigned char i;
    unsigned char *ptr;

    // Copy paddle sprite to $3000 (block 192)
    ptr = (unsigned char*)SPRITE_DATA;
    for (i = 0; i < 63; i++) ptr[i] = paddle_data[i];

    // Copy ball sprite to $3040 (block 193)
    ptr = (unsigned char*)(SPRITE_DATA + 64);
    for (i = 0; i < 63; i++) ptr[i] = ball_data[i];

    // Set sprite pointers
    SPRITE_PTRS[0] = 192;  // Paddle
    SPRITE_PTRS[1] = 193;  // Ball

    // Enable both sprites
    VIC_SPR_ENA = 0x03;

    // Colors
    SPR0_COL = LTBLUE;
    SPR1_COL = WHITE;

    // No expansion, no multicolor
    VIC_SPR_DBL_X = 0;
    VIC_SPR_DBL_Y = 0;
    VIC_SPR_MCOLOR = 0;
    VIC_SPR_HI_X = 0;
}

void draw_brick(unsigned char row, unsigned char col, unsigned char visible) {
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *clr = (unsigned char*)0xD800;
    unsigned int pos;
    unsigned char i;
    unsigned char chr = visible ? 160 : 32;
    unsigned char color = row_colors[row];

    pos = (BRICK_START_Y + row) * 40 + BRICK_START_COL + (col * BRICK_WIDTH);
    for (i = 0; i < BRICK_WIDTH; i++) {
        scr[pos + i] = chr;
        clr[pos + i] = color;
    }
}

void init_bricks(void) {
    unsigned char r, c;
    bricks_left = 0;
    for (r = 0; r < BRICK_ROWS; r++) {
        for (c = 0; c < BRICK_COLS; c++) {
            bricks[r][c] = 1;
            bricks_left++;
            draw_brick(r, c, 1);
        }
    }
}

void draw_field(void) {
    unsigned char y;
    unsigned char *scr = (unsigned char*)0x0400;
    unsigned char *clr = (unsigned char*)0xD800;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    // Draw left wall (column 0)
    for (y = 1; y < 25; y++) {
        scr[y * 40] = 0xE1;  // Vertical bar
        clr[y * 40] = GREY;
    }

    // Draw right wall (column 29)
    for (y = 1; y < 25; y++) {
        scr[y * 40 + 29] = 0xE1;
        clr[y * 40 + 29] = GREY;
    }

    // Draw top wall (row 1, columns 0-29)
    for (y = 0; y < 30; y++) {
        scr[40 + y] = 0xC0;  // Horizontal bar
        clr[40 + y] = GREY;
    }
}

void init_game(void) {
    // Center paddle in play area
    paddle_x = WALL_LEFT + 90;  // Center-ish

    // Ball on paddle
    ball_x = paddle_x + 6;
    ball_y = PADDLE_Y - 12;
    ball_dx = 2;
    ball_dy = -2;
    serve_mode = 1;
}

void update_sprites(void) {
    SPR0_X = paddle_x;
    SPR0_Y = PADDLE_Y;
    SPR1_X = ball_x;
    SPR1_Y = ball_y;
}

void draw_status(void) {
    // Score on right side (columns 30-39)
    gotoxy(30, 2);
    textcolor(WHITE);
    cprintf("SCORE");
    gotoxy(30, 3);
    cprintf("%05u", score);

    gotoxy(30, 6);
    textcolor(YELLOW);
    cprintf("LEVEL");
    gotoxy(32, 7);
    cprintf("%u", level);

    gotoxy(30, 10);
    textcolor(RED);
    cprintf("LIVES");
    gotoxy(32, 11);
    cprintf("%u", lives);
}

void read_input(void) {
    unsigned char joy = joy_read(JOY_2);
    unsigned char speed = 4;

    // Paddle boundaries: can move from WALL_LEFT to WALL_RIGHT - PADDLE_WIDTH
    if (JOY_LEFT(joy) && paddle_x > WALL_LEFT + 4) {
        paddle_x -= speed;
    }
    if (JOY_RIGHT(joy) && paddle_x < 248 - PADDLE_WIDTH - 4) {
        paddle_x += speed;
    }

    // Ball follows paddle in serve mode
    if (serve_mode) {
        ball_x = paddle_x + (PADDLE_WIDTH / 2) - (BALL_SIZE / 2);
        ball_y = PADDLE_Y - 12;

        if (JOY_FIRE(joy)) {
            serve_mode = 0;
            ball_dx = (rand() & 1) ? 2 : -2;
            ball_dy = -2;
        }
    }
}

unsigned char check_brick_hit(unsigned char bx, unsigned char by) {
    unsigned char scr_col, scr_row;
    int brick_row, brick_col;
    int rel_col;

    // Convert sprite X to screen column
    // Sprite X = 24 + col*8, so col = (X - 24) / 8
    if (bx < 24) return 0;
    scr_col = (bx - 24) / 8;

    // Convert sprite Y to screen row
    if (by < 50) return 0;
    scr_row = (by - 50) / 8;

    // Check if in brick area
    if (scr_row < BRICK_START_Y || scr_row >= BRICK_START_Y + BRICK_ROWS) {
        return 0;
    }

    // Calculate brick coordinates
    brick_row = scr_row - BRICK_START_Y;
    rel_col = scr_col - BRICK_START_COL;
    if (rel_col < 0) return 0;
    brick_col = rel_col / BRICK_WIDTH;

    if (brick_col < 0 || brick_col >= BRICK_COLS) return 0;

    if (bricks[brick_row][brick_col]) {
        bricks[brick_row][brick_col] = 0;
        bricks_left--;
        draw_brick(brick_row, brick_col, 0);
        score += 10 * level;
        sound_bounce(0x30 + brick_row * 8);
        return 1;
    }
    return 0;
}

void move_ball(void) {
    unsigned char new_x, new_y;
    unsigned char hit = 0;
    signed char paddle_center_offset;

    if (serve_mode) return;

    new_x = ball_x + ball_dx;
    new_y = ball_y + ball_dy;

    // Left wall collision (sprite X = 32 is column 1)
    if (new_x <= WALL_LEFT + 8) {
        ball_dx = -ball_dx;
        new_x = WALL_LEFT + 9;
        sound_bounce(0x20);
    }

    // Right wall collision (column 29 = sprite X 256, but we use 248)
    if (new_x >= 248 - BALL_SIZE) {
        ball_dx = -ball_dx;
        new_x = 248 - BALL_SIZE - 1;
        sound_bounce(0x20);
    }

    // Top wall collision
    if (new_y <= WALL_TOP) {
        ball_dy = -ball_dy;
        new_y = WALL_TOP + 1;
        sound_bounce(0x28);
    }

    // Brick collision - check ball corners
    hit = check_brick_hit(new_x + 2, new_y + 2);
    if (!hit) hit = check_brick_hit(new_x + BALL_SIZE - 2, new_y + 2);
    if (!hit) hit = check_brick_hit(new_x + 2, new_y + BALL_SIZE - 2);
    if (!hit) hit = check_brick_hit(new_x + BALL_SIZE - 2, new_y + BALL_SIZE - 2);

    if (hit) {
        ball_dy = -ball_dy;
    }

    // Paddle collision
    if (new_y >= PADDLE_Y - 10 && ball_dy > 0) {
        if (new_x + BALL_SIZE >= paddle_x && new_x <= paddle_x + PADDLE_WIDTH) {
            // Calculate angle based on where ball hits paddle
            paddle_center_offset = (signed char)((new_x + BALL_SIZE/2) - (paddle_x + PADDLE_WIDTH/2));
            ball_dx = paddle_center_offset / 3;
            if (ball_dx == 0) ball_dx = (rand() & 1) ? 1 : -1;
            if (ball_dx > 3) ball_dx = 3;
            if (ball_dx < -3) ball_dx = -3;
            ball_dy = -2;
            new_y = PADDLE_Y - 11;
            sound_bounce(0x38);
        }
    }

    // Ball lost (below paddle)
    if (new_y > PADDLE_Y + 20) {
        lives--;
        sound_bounce(0x10);

        if (lives == 0) {
            gotoxy(10, 12);
            textcolor(RED);
            cprintf("GAME OVER!");
            gotoxy(6, 14);
            textcolor(WHITE);
            cprintf("PRESS FIRE TO RESTART");

            while (!(joy_read(JOY_2) & 0x10)) wait_vblank();

            // Reset game
            score = 0;
            lives = 3;
            level = 1;
            draw_field();
            init_bricks();
            draw_status();
        }
        init_game();
        return;
    }

    ball_x = new_x;
    ball_y = new_y;
}

int main(void) {
    unsigned char frame = 0;

    clrscr();
    bgcolor(BLACK);
    bordercolor(BLUE);

    init_sound();
    init_sprites();
    joy_install(joy_static_stddrv);

    // Title screen
    gotoxy(10, 6);
    textcolor(YELLOW);
    cprintf("B R E A K O U T");

    gotoxy(8, 9);
    textcolor(WHITE);
    cprintf("FOR COMMODORE 64");

    gotoxy(6, 13);
    textcolor(CYAN);
    cprintf("PRESS FIRE TO START");

    gotoxy(8, 16);
    textcolor(GREEN);
    cprintf("JOYSTICK PORT 2");

    // Wait for fire
    while (!(joy_read(JOY_2) & 0x10)) wait_vblank();

    // Debounce
    frame = 0;
    while (frame < 15) { wait_vblank(); frame++; }

    // Game init
    score = 0;
    lives = 3;
    level = 1;

    draw_field();
    init_bricks();
    init_game();
    update_sprites();
    draw_status();

    // Main loop
    while (1) {
        wait_vblank();
        frame++;

        if ((frame & 7) == 0) sound_off();

        read_input();
        move_ball();
        update_sprites();

        // Update status occasionally
        if ((frame & 31) == 0) {
            draw_status();
        }

        // Level complete
        if (bricks_left == 0) {
            level++;
            score += 100 * level;

            gotoxy(10, 12);
            textcolor(YELLOW);
            cprintf("LEVEL %u COMPLETE!", level);

            frame = 0;
            while (frame < 120) { wait_vblank(); frame++; }

            draw_field();
            init_bricks();
            init_game();
            draw_status();
        }
    }

    return 0;
}
